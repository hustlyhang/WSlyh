#ifndef LOG_H
#define LOG_H
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>//getpid, gettid

// 优化获取时间

struct UTC_Time{

    UTC_Time() {
        struct timeval tt;
        gettimeofday(&tt, NULL);
        m_cache_sec = tt.tv_sec;
        m_cache_min = m_cache_sec / 60;
        struct tm cut_tm;
        localtime_r((time_t *)&m_cache_sec, &cur_tm);
        m_year = cur_tm.tm_year;
        m_month = cur_tm.tm_;

        m_year = cur_tm.tm_year;

        

        
    }



private:
    int m_year, m_month, m_day, m_hour, m_minute, m_sec;
    uint64_t m_cache_sec, m_cache_min;      // 用来保存待比较的秒和分，如果相等就直接用缓存，不相等再更新
}; 





#endif