#include "ThreadPool.h"

CThreadPool::CThreadPool(int _tasksize, int _threadssize) {
    // 初始化线程数组和任务数组大小
    tasksize = _tasksize;
    threadssize = _threadssize;
    // 初始化条件变量和互斥锁
    pthread_mutex_init(&m_mutex, NULL);
    pthread_cond_init(&m_cond, NULL);
    
}