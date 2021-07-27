#include "util.h"
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <string.h>


/*
    * 读取文件描述符上的数据，返回读取的长度，如果失败返回-1
*/
ssize_t readn(int fd, void *buff, size_t n)
{
    size_t nleft = n;
    ssize_t nread = 0;
    ssize_t readSum = 0;
    char *ptr = (char*)buff;
    while (nleft > 0)
    {
        if ((nread = read(fd, ptr, nleft)) < 0)
        {
            if (errno == EINTR)
                nread = 0;
            else if (errno == EAGAIN)
            {
                return readSum;
            }
            else
            {
                return -1;
            }  
        }
        else if (nread == 0)
            break;
        readSum += nread;
        nleft -= nread;
        ptr += nread;
    }
    return readSum;
}

ssize_t writen(int fd, void *buff, size_t n)
{
    size_t nleft = n;
    ssize_t nwritten = 0;
    ssize_t writeSum = 0;
    char *ptr = (char*)buff;
    while (nleft > 0)
    {
        if ((nwritten = write(fd, ptr, nleft)) <= 0)
        {
            if (nwritten < 0)
            {
                if (errno == EINTR || errno == EAGAIN)
                {
                    nwritten = 0;
                    continue;
                }
                else
                    return -1;
            }
        }
        writeSum += nwritten;
        nleft -= nwritten;
        ptr += nwritten;
    }
    return writeSum;
}

//todo 对管道的处理？？
/*
    * 当服务器close一个连接时，若client端接着发数据。根据TCP协议的规定，会收到一个RST响应，
    * client再往这个服务器发送数据时，系统会发出一个SIGPIPE信号给进程，告诉进程这个连接已
    * 经断开了，不要再写了。
    * 所以, 第二次调用write方法(假设在收到RST之后), 会生成SIGPIPE信号, 导致进程退出.
    * 为了避免进程退出, 可以捕获SIGPIPE信号, 或者忽略它, 给它设置SIG_IGN信号处理函数:
    * signal(SIGPIPE, SIG_IGN);
    * 这样, 第二次调用write方法时, 会返回-1, 同时errno置为SIGPIPE. 程序便能知道对端已经关闭.
*/
void handle_for_sigpipe()
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0; //? 相当于忽略这个值
    if(sigaction(SIGPIPE, &sa, NULL))
        return;
}

int setSocketNonBlocking(int fd)
{
    //? fcntl 文件描述符控制
    int flag = fcntl(fd, F_GETFL, 0);
    //? F_GETFL 返回文件的访问方法和状态，第三个参数忽略
    if(flag == -1)
        return -1;

    flag |= O_NONBLOCK;
    //? 将flag设置为非阻塞
    //? F_SETFL 将第三个参数设置给文件
    if(fcntl(fd, F_SETFL, flag) == -1)
        return -1;
    return 0;
}