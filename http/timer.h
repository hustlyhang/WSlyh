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

class CHeapTimer;

struct SClientData {
    sockaddr_in address;        // 连接的地址
    int sock_fd;                // http连接的文件描述符
    CHeapTimer* timer;           // 小根堆时间节点
};

class CHeapTimer {
public:
    CHeapTimer(int _delaytime);
    // 需要一个回调函数
    void (*callback_func)(SClientData*);
    time_t m_iExpireTime;   // 定时器过期时间
    SClientData* m_sClientData;
    int m_iPos;                // 位于数组堆中的下标
};


// 定时器小根堆
class CTimerHeap{
public:
    CTimerHeap() {m_iCapacity = 10;}
    CTimerHeap(int _capacity);
    ~CTimerHeap();

public:
    void HeapDown(int heap_node);// 向下调整
    void AddTimer(CHeapTimer*);
    void DelTimer(CHeapTimer*);
    void PopTimer();
    void Tick();
    void Adjust(CHeapTimer*);      // 向下调整
    void Resize(); // 当数组容纳不下的时候扩容

    CHeapTimer* GetTop();

private:
    int m_iCapacity;
    int m_iCurNum;
    CHeapTimer** m_aTimers;
};


#endif