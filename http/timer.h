#ifndef TIMER_H
#define TIMER_H
#include "http.h"
#include <vector>
#include <sys/time.h>
#include <queue>
#include <unistd.h>
#include <iostream>
#include <vector>

// 每个timer都包含过期时间以及相绑定的用户数据
struct timer
{
    timer(){};
    struct timeval expire_time;
    http* user_data;
};
struct cmp {
    bool operator () (timer a, timer b) {
        return a.expire_time.tv_sec > b.expire_time.tv_sec;
    }
};

std::priority_queue<timer*, std::vector<timer*>, cmp > top_timer


#endif