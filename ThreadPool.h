#pragma once
// 线程池
#include <pthread.h>

// 任务结构体，包含执行函数和相应的参数
typedef struct{
    void (*func)(void *);
    void *argument;
}STask;


class CThreadPool{
  private:
    STask*              m_arryTasks;                // 任务数组
    pthread_t*          m_arryThreads;              // 线程数组
    pthread_cond_t      m_cond;                     // 条件变量
    pthread_mutex_t     m_mutex;                    // 互斥锁
    int                 head, tail;                 // 利用两个变量存储任务数组中的任务情况
    int                 started;                    // 记录分配了任务的线程数
    int                 tasksize,threadssize;       // 任务数组和线程数组的大小
  public:
    CThreadPool(int _tasksize, int _threadssize);
    ~CThreadPool();
    
    void AddTask(STask m_sTask);    // 添加任务
    void ThreadBegin();             // 线程执行函数
    
    
};

