#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>
#include <list>
#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"

template<class T>
class threadpool {
public:
    /*
        thread_number 是线程池中线程的数量， max_requests是请求队列中最多允许的、等待处理的请求的数量
    */
    threadpool(connection_pool* connPool, int thread_number = 8, int max_request = 10000);
    ~threadpool();
    bool append(T* request);

private:
    static void* worker(void* arg);     // 工作线程运行的函数，不断从工作队列中取出任务并执行
    void run();

private:
    int m_thread_number;                // 线程池中线程数目
    int m_max_requests;                 // 请求队列中允许的最大请求数
    pthread_t* m_threads;               // 线程数组，最大值为m_thread_number
    std::list<T* > m_workqueue;         // 请求队列
    locker m_queuelocker;               // 保护请求队列的互斥锁
    sem m_queuestat;                    // 是否有任务需要处理
    bool m_stop;                        // 是否结束线程
    connection_pool* m_connPool;        // 数据库
};

template<class T>
threadpool<T>::threadpool(connection_pool* connPool, int thread_number = 8, int max_request = 10000){
    if (thread_number <= 0 || max_request <= 0) throw std::exception();
    m_threads = new pthread_t[thread_number];

    if (m_threads == NULL) throw std::exception();
    for (int i = 0; i < thread_number; ++i) {
        if (pthread_create(m_threads + i, NULL, worker, this) != 0){
            delete [] m_threads;
            throw std::exception();
        }
        if (pthread_detach(m_threads[i]) != 0) {
            delete [] m_threads;
            throw std::exception();
        }
    }
}

template<class T>
threadpool<T>::~threadpool(){
    delete [] m_threads;
    m_stop = true;
}

template<class T>
bool threadpool<T>::append(T* req){

    m_queuelocker.lock();
    if (m_workqueue.size() > m_max_requests) {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(req);
    m_queuelocker.unlock();

    // 添加了新任务后需要将信号量加一
    m_queuestat.post();
}

template<class T>
void* threadpool<T>::worker(void* arg){
    threadpool* pool = (threadpool* )arg;
    pool->run();
    return pool;
}

// ! 这儿m_queuestat不就是信号量，表示任务的个数，为什么在其中还要对工作队列进行判断呢
template<class T>
void threadpool<T>::run(){
    while (!m_stop) {
        m_queuestat.wait();
        m_queuelocker.lock();

        if (m_workqueue.empty()) {
            m_queuelocker.unlock();
            continue;
        }
        T* request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();

        if (!request) 
            continue;

        connectionRAII mysqlcon(&request->mysql, m_connPool);
        request->process();
    }
}
#endif // !THREADPOOL_H