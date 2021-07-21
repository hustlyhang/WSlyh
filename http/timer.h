#ifndef TIMER_H
#define TIMER_H
#include "http.h"
#include <vector>
#include <sys/time.h>
#include <netinet/in.h>
#include <queue>
#include <unistd.h>
#include <iostream>
#include <vector>

class HeapTimer;

struct client_data {
    sockaddr_in address;        // 连接的地址
    int sock_fd;                // http连接的文件描述符
    HeapTimer* timer;           // 小根堆时间节点
};

class HeapTimer {
public:
    HeapTimer(int delaytime);
    // 需要一个回调函数
    void (*callback_func)(client_data*);
public:
    time_t expire_time;   // 定时器过期时间
    client_data* m_client_data;
};


// 定时器小根堆
class TimerHeap{
public:
    TimerHeap(int _capacity);
    ~TimerHeap();

public:
    void heap_down(int heap_node);// 向下调整
    void add_timer(HeapTimer*);
    void del_timer(HeapTimer*);
    void pop_timer();
    void tick();

    void resize(); // 当数组容纳不下的时候扩容


private:
    int m_capacity;
    int m_cur_num;
    HeapTimer** m_timer_array;
};







#endif