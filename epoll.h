#ifndef EPOLL
#define EPOLL
#include "requestData.h"
#include <stdint.h>

// 每次返回的最大事件数字
const int MAX_EVENT_SIZE = 10000;


int epoll_init();
int epoll_add(int epoll_fd, int fd, void* request, __uint32_t events);
int epoll_chm(int epoll_fd, int fd, void* request, __uint32_t events);
int epoll_del(int epoll_fd, int fd, void* request, __uint32_t events);
int my_epoll_wait(int epoll_fd, struct epoll_event* events, int max_events, int timeout);



#endif // !EPOLL

