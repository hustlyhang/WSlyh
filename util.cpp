#include "util.h"
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>


/*
    读取数据，当读取完指定数据或者为信号EAGAIN时说明读取完成
*/
ssize_t readn(int fd, char* buf, size_t n){
    ssize_t ans = 0, leftover = n;

    char* pos = buf;
    while (leftover > 0) {
        ssize_t ret = read(fd, pos, leftover);
        //? 判断是否读取成功
        if (ret < 0) {
            //? 读取失败
            //? 判断错误
            if (errno == EINTR) {
                //? 系统中断
                ret = 0;
            }
            else if (errno == EAGAIN) {
                //? 读取完成
                return ans;
            }
            else {
                //? 出错
                return -1;
            }
        }
        ans += ret;
        pos += ret;
        leftover -= ret;
    }
    return ans;
}
/*
    写入数据，因为知道要写入多少数据，所以当有信号EAGAIN时仍需要继续写入
*/
ssize_t writen(int fd, char* buf, size_t n){
    ssize_t ans = 0, leftover = n;

    char* pos = buf;
    while (leftover > 0) {
        ssize_t ret = write(fd, pos, leftover);
        //? 判断是否写入成功
        if (ret < 0) {
            //? 读取失败
            //? 判断错误
            if (errno == EINTR || errno == EAGAIN) {
                //? 写入中断，需要再次写入
                ret = 0;
                continue;
            }
            else {
                //? 出错
                return -1;
            }
        }
        ans += ret;
        pos += ret;
        leftover -= ret;
    }
    return ans;
}

/*
    处理SIGPIPE信号,防止线程退出
    成功返回0，失败返回-1；
*/
int hand_sigpipe(){
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_flags = 0;
    sa.sa_handler = SIG_IGN;
    int ret = sigaction(SIGPIPE, &sa, NULL);
    if (ret < 0) return -1;
    return 0;
}
/*
    将文件描述符设置为非阻塞
    成功返回0，失败返回-1；
*/
int setSocketNonBlocking(int fd){
    int flg = fcntl(fd, F_GETFL, 0);
    if (flg < 0) return -1;
    flg |= O_NONBLOCK;
    int ret = fcntl(fd, flg, 0);
    if (ret < 0) return -1;
    return 0;
}
