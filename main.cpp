#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>
#include <signal.h>
#include "./lock/locker.h"
#include "./http/http.h"
#include "./http/timer.h"
#include "./threadpool/threadpool.h"
#include "./log/log.h"

#define MAX_FD 65536                // 最大文件描述符
#define MAX_EVENT_NUMBER 10000      //最大事件数
#define TIMESLOT 5                  //最小超时单位

#define listenfdLT

extern void AddFd(int _epollFd, int _fd, bool _oneShot, int _triggerMode);
extern void RemoveFd(int _epollFd, int _fd);
extern void ModFd(int _epollFd, int _fd, int _event, int _triggerMode);
extern int SetNonBlocking(int _fd);

// 通过管道来处理定时器信号
static int pipeFd[2];
static CTimerHeap cTimerHeap(1000);
static int epollFd = 0;

// 信号处理
void SigHandler(int _sig) {
    int oldErrno = errno;
    int msg = _sig;
    send(pipeFd[1], (char *)&msg, 1, 0);
    errno = oldErrno;
}

// 信号处理函数
void AddSig(int _sig, void(_handler)(int), bool _restart = true) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = _handler;
    if (_restart) {
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(_sig, &sa, NULL) != -1);
}

// 时间堆处理
void TimerHandler() {
    cTimerHeap.Tick();
    alarm(TIMESLOT);
}

void CallBackFunc(SClientData *_userData) {
    LOG_TRACE("CallBackFunc");
    epoll_ctl(epollFd, EPOLL_CTL_DEL, _userData->sock_fd, 0);
    assert(_userData);
    // 只要关闭相关的文件描述符，chttp类可以复用
    close(_userData->sock_fd);
    CHttp::m_iHttpCnt--;
    LOG_INFO("close fd %d", _userData->sock_fd);
}

void SendInfo(int _conn, const char* _info) {
    send(_conn, _info, strlen(_info), 0);
    close(_conn);
}

int main(int argc, char* argv[]) {
    LOG_TRACE("Begin");
    if (argc <= 1) {
        printf("usage: %s ip_address port_number\n", basename(argv[0]));
        return 1;
    }
    int level = atoi(argv[2]);
    // 初始化日志
    LOG_INIT("./ServerLog", "WebServer", level); 
    int port = atoi(argv[1]);
    LOG_INFO("port is : %d", port);
    AddSig(SIGPIPE, SIG_IGN);

    // 创建线程池
    CThreadPool<CHttp>* pool = nullptr;
    try {
        pool = new CThreadPool<CHttp>(4);
    }
    catch(...) {
        LOG_ERROR("construct threadpool failed!");
        return 1;
    }

    CHttp* https = new CHttp[MAX_FD]();
    if (not https)
        LOG_ERROR("construct array chttp failed!!");
    assert(https);

    int listenFd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenFd >= 0);

    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    int flag = 1;
    setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    ret = bind(listenFd, (struct sockaddr *)&address, sizeof(address));
    assert(ret >= 0);
    ret = listen(listenFd, 5);
    assert(ret >= 0);

    //创建内核事件表
    epoll_event events[MAX_EVENT_NUMBER];
    epollFd = epoll_create(5);
    assert(epollFd != -1);

    // et，oneshot
    AddFd(epollFd, listenFd, false, 0);
    CHttp::m_iEpollFd = epollFd;

    // 创建管道
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipeFd);
    assert(ret != -1);
    SetNonBlocking(pipeFd[1]);
    AddFd(epollFd, pipeFd[0], false, 0);

    AddSig(SIGALRM, SigHandler, false);
    AddSig(SIGTERM, SigHandler, false);
    bool stop_server = false;

    SClientData* clientDatas = new SClientData[MAX_FD]();

    bool timeout = false;
    alarm(TIMESLOT);

    while (!stop_server) {
        LOG_TRACE("eventloop");
        LOG_TRACE("begin epoll_wait");
        int number = epoll_wait(epollFd, events, MAX_EVENT_NUMBER, -1);
        LOG_TRACE("end epoll_wait");
        if (number < 0 && errno != EINTR) {
            LOG_ERROR("epoll wait failed!!!");
            break;
        }
        LOG_INFO("epoll_wait number : %d", number);
        // 遍历
        for (int i = 0; i < number; ++i) {
            int sockfd = events[i].data.fd;
            LOG_INFO("epoll_wait sockfd : %d", sockfd);
            if (sockfd == listenFd) {
                // 新到的客户连接
                LOG_TRACE("new socket come");
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
#ifdef listenfdLT
                LOG_TRACE("listenFd in Lt model");
                int connfd = accept(listenFd, (struct sockaddr *)&client_address, &client_addrlength);
                LOG_INFO("Add sockfd : %d", connfd);
                if (connfd < 0) {
                    LOG_ERROR("%s:errno is:%d", "accept error", errno);
                    continue;
                }
                if (CHttp::m_iHttpCnt >= MAX_FD) {
                    SendInfo(connfd, "Internal server busy");
                    LOG_ERROR("Internal server busy");
                    continue;
                }
                // 复用
                https[connfd].Init(connfd, client_address, 0);
                clientDatas[connfd].address = client_address;
                clientDatas[connfd].sock_fd = connfd;

                CHeapTimer* tempTimer = new CHeapTimer(3 * TIMESLOT);
                tempTimer->m_sClientData = &clientDatas[connfd];
                tempTimer->callback_func = CallBackFunc;
                clientDatas[connfd].timer = tempTimer;
                cTimerHeap.AddTimer(tempTimer);
#endif

#ifdef listenfdET
                LOG_TRACE("listenFd in ET model");
                while (true) {
                    int connfd = accept(listenFd, (struct sockaddr *)&client_address, &client_addrlength);
                    if (connfd < 0) {
                        LOG_ERROR("%s:errno is:%d", "accept error", errno);
                        break;
                    }
                    if (CHttp::m_iHttpCnt >= MAX_FD) {
                        SendInfo(connfd, "Internal server busy");
                        LOG_ERROR("Internal server busy");
                        break;
                    }
                    https[connfd].Init(connfd, client_address, 1);
                    clientDatas[connfd].address = client_address;
                    clientDatas[connfd].sock_fd = connfd;

                    time_t cur = time(NULL);
                    CHeapTimer* tempTimer = new CHeapTimer(cur + 3 * TIMESLOT);
                    tempTimer->m_sClientData = &clientDatas[connfd];
                    tempTimer->callback_func = CallBackFunc;
                    clientDatas[connfd].timer = tempTimer;
                    cTimerHeap.AddTimer(tempTimer);
                }
                continue;
#endif          
            }
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                LOG_ERROR("somethin wrong happen");
                CHeapTimer* tempTimer = clientDatas[sockfd].timer;
                tempTimer->callback_func(&clientDatas[sockfd]);
                if (tempTimer)
                    cTimerHeap.DelTimer(tempTimer);
            }
            else if ((sockfd == pipeFd[0]) && (events[i].events & EPOLLIN)) {
                // 管道发送的信号
                int sig;
                char signals[1024];
                ret = recv(pipeFd[0], signals, sizeof(signals), 0);
                if (ret == -1) {
                    continue;
                }
                else if (ret == 0) {
                    continue;
                }
                else {
                    for (int i = 0; i < ret; ++i) {
                        switch (signals[i]) {
                        case SIGALRM:
                        {
                            // AddFd(epollFd, pipeFd[0], true, 0);
                            timeout = true;
                            break;
                        }
                        case SIGTERM:
                        {
                            stop_server = true;
                        }
                        }
                        LOG_INFO("deal signals: %d", signals[i]);
                    }
                }
            }
            else if (events[i].events & EPOLLIN) {
                // socket上有数据可以读取了，调用chttp类的Read
                LOG_TRACE("data coming EPOLLIN");
                CHeapTimer* tempTimer = clientDatas[sockfd].timer;
                if (https[sockfd].Read()) {
                    LOG_TRACE("Read client addr : %s", inet_ntoa(https[sockfd].GetAddress()->sin_addr));
                    pool->Append(https + sockfd);
                    if (tempTimer) {
                        LOG_TRACE("adjust timer once");
                        time_t cur = time(NULL);
                        tempTimer->m_iExpireTime = cur + 3 * TIMESLOT;
                        cTimerHeap.Adjust(tempTimer);
                    }
                }
                else {
                    if (tempTimer->callback_func) {
                        tempTimer->callback_func(&clientDatas[sockfd]);
                        if (tempTimer) cTimerHeap.DelTimer(tempTimer);
                    }
                }
            }
            else if (events[i].events & EPOLLOUT) {
                LOG_TRACE("data can write EPOLLOUT");
                // socket上可以写数据了，调用chttp类的Write
                CHeapTimer* tempTimer = clientDatas[sockfd].timer;
                if (https[sockfd].Write()) {
                    LOG_TRACE("Write client addr : %s", inet_ntoa(https[sockfd].GetAddress()->sin_addr));
                    if (tempTimer) {
                        LOG_TRACE("adjust timer once");
                        time_t cur = time(NULL);
                        tempTimer->m_iExpireTime = cur + 3 * TIMESLOT;
                        cTimerHeap.Adjust(tempTimer);
                    }
                }
                else {
                    if (tempTimer && &clientDatas[sockfd]) {
                        tempTimer->callback_func(&clientDatas[sockfd]);
                    }
                    if (tempTimer) cTimerHeap.DelTimer(tempTimer);
                }
            }
        }
        if (timeout) {
            TimerHandler();
            timeout = false;
        }
        LOG_INFO("End");
    }
    close(epollFd);
    close(listenFd);
    close(pipeFd[1]);
    close(pipeFd[0]);
    delete [] https;
    delete [] clientDatas;
    delete pool;
    return 0;
}
