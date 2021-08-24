#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>
#include <queue>
#include <memory>
#include <string>
#include "../lock/locker.h"
#include <unistd.h>
#include "../log/log.h"

#define TASK_NUM 100
#define MAX_THREAD_NUM 16

template <class T>
class CThreadPool
{
public:
    CThreadPool(int thread_num);
    ~CThreadPool();
    bool Append(T *request_data);
    // 线程入口函数,会不断得从任务队列中取出函数然后执行
    static void *Work(void *pool);

private:
    // 锁和条件变量
    CLocker m_cLock;
    CCond m_cCond;
    pthread_t *m_aThreads;
    std::queue<T *> m_qTasks;
};

template <class T>
CThreadPool<T>::CThreadPool(int thread_num) {
    if (thread_num > MAX_THREAD_NUM) {
        LOG_INFO("the threadnum is too big, limit :%d", MAX_THREAD_NUM);
        return;
    }
    m_aThreads = new pthread_t[thread_num];

    LOG_INFO("Construct ThreadPool");

    for (int i = 0; i < thread_num; ++i) {
        if (pthread_create(m_aThreads + i, NULL, Work, this) != 0) {
            delete[] m_aThreads;
            LOG_ERROR("create threads error!");
            return;
        }

        if (pthread_detach(m_aThreads[i]) != 0) {
            delete[] m_aThreads;
            LOG_ERROR("detach threads error! %d", i);
            return;
        }
    }
    LOG_INFO("Construct ThreadPool Success");
}

template <class T>
void *CThreadPool<T>::Work(void *arg) {
    CThreadPool *m_pool = (CThreadPool *)arg;
    while (true) {
        m_pool->m_cLock.lock();
        // 之前卡住是因为当Append一个任务进去后，发送了signal，但是这边还没有wait，所以就会导致死锁，所以要加一个while
        while (m_pool->m_qTasks.empty()) {
            m_pool->m_cCond.wait(m_pool->m_cLock.get());
        }
        T *m_task = m_pool->m_qTasks.front();
        m_pool->m_qTasks.pop();
        m_task->HttpParse();
        m_pool->m_cLock.unlock();
    }
}

template <class T>
bool CThreadPool<T>::Append(T *task) {
    // 怎么获取锁并且插入任务
    if (m_qTasks.size() >= TASK_NUM) {
        LOG_INFO("Too much tasks, the limit is %d", TASK_NUM);
        return false;
    }
    m_cLock.lock();
    m_qTasks.push(task);
    m_cLock.unlock();
    m_cCond.signal();
    LOG_INFO("Append task success");
    return true;
}

template <class T>
CThreadPool<T>::~CThreadPool() {
    delete[] m_aThreads;
}

#endif