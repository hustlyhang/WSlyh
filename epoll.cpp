#include "epoll.h"
#include <sys/epoll.h>
#include <errno.h>
#include <stdio.h>


// 需要一个数组来存放每次epoll_wait返回的事件
struct epoll_event* events;
/*
    初始化epoll
*/
int epoll_init() {
    int epoll_fd = epoll_create(1);
    if (epoll_fd < 0) {
        perror("epoll_init fail!");
        return -1;
    }
    // 初始化数组
    events = new epoll_event[MAX_EVENT_SIZE];
    return epoll_fd;
}


/*
    向epoll中注册文件描述符上的事件
*/
int epoll_add(int epoll_fd, int fd, void* request, __uint32_t events) {
    struct epoll_event ee;
    ee.events = events;
    ee.data.ptr = request;
    int ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ee);
    if (ret < 0) {
        perror("epoll_add fail!");
        return -1;
    }
    return 0;
}

/*
    向epoll中修改文件描述符上的事件
*/

int epoll_chm(int epoll_fd, int fd, void* request, __uint32_t events) {
    struct epoll_event ee;
    ee.events = events;
    ee.data.ptr = request;
    int ret = epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ee);
    if (ret < 0) {
        perror("epoll_chm fail!");
        return -1;
    }
    return 0;
}
/*
    向epoll中删除文件描述符上的事件
*/

int epoll_chm(int epoll_fd, int fd, void* request, __uint32_t events) {
    struct epoll_event ee;
    ee.events = events;
    ee.data.ptr = request;
    int ret = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, &ee);
    if (ret < 0) {
        perror("epoll_del fail!");
        return -1;
    }
    return 0;
}


int my_epoll_wait(int epoll_fd, struct epoll_event* events, int max_events, int timeout) {
    int ret = epoll_wait(epoll_fd, events, max_events, timeout);
    if (ret < 0) {
        perror("epoll_wait fail!");
        return -1;
    }
    return ret;
}