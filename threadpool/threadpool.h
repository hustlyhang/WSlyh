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
class threadpool
{
public:
    threadpool(int thread_num);
    ~threadpool();
    // 向工作队列中插入任务,flg代表任务类型
    // 1 写，   0 读
    // bool append(T *request_data, int flg);
    bool append(T *request_data);
    // 线程入口函数,会不断得从任务队列中取出函数然后执行
    static void *work(void *pool);

private:
    // 锁和条件变量
    CLocker m_lock;
    CCond m_cond;
    pthread_t *m_threads;
    std::queue<T *> m_tasks;
};

template <class T>
threadpool<T>::threadpool(int thread_num)
{
    if (thread_num > MAX_THREAD_NUM) {
        LOG_INFO("the threadnum is too big, limit :%d", MAX_THREAD_NUM);
        return;
    }
    m_threads = new pthread_t[thread_num];

    LOG_INFO("Construct ThreadPool");

    for (int i = 0; i < thread_num; ++i)
    {
        if (pthread_create(m_threads + i, NULL, work, this) != 0) {
            delete[] m_threads;
            LOG_ERROR("create threads error!");
            return;
        }

        if (pthread_detach(m_threads[i]) != 0) {
            delete[] m_threads;
            LOG_ERROR("detach threads error! %d", i);
            return;
        }
    }
    LOG_INFO("Construct ThreadPool Success");
}

template <class T>
void *threadpool<T>::work(void *arg) {
    threadpool *m_pool = (threadpool *)arg;
    while (true) {
        m_pool->m_lock.lock();
        // 之前卡住是因为当append一个任务进去后，发送了signal，但是这边还没有wait，所以就会导致死锁，所以要加一个while
        while (m_pool->m_tasks.empty()) {
            m_pool->m_cond.wait(m_pool->m_lock.get());
        }
        T *m_task = m_pool->m_tasks.front();
        m_pool->m_tasks.pop();
        m_task->HttpParse();
        m_pool->m_lock.unlock();
    }
}

template <class T>
bool threadpool<T>::append(T *task)
{
    // 怎么获取锁并且插入任务
    if (this->m_tasks.size() >= TASK_NUM) {
        return false;
    }
    m_lock.lock();
    m_tasks.push(task);
    m_lock.unlock();
    m_cond.signal();
    LOG_INFO("Append task success");
    return true;
}

template <class T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
}

#endif