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
        thread_number ���̳߳����̵߳������� max_requests������������������ġ��ȴ���������������
    */
    threadpool(connection_pool* connPool, int thread_number = 8, int max_request = 10000);
    ~threadpool();
    bool append(T* request);

private:
    static void* worker(void* arg);     // �����߳����еĺ��������ϴӹ���������ȡ������ִ��
    void run();

private:
    int m_thread_number;                // �̳߳����߳���Ŀ
    int m_max_requests;                 // �����������������������
    pthread_t* m_threads;               // �߳����飬���ֵΪm_thread_number
    std::list<T* > m_workqueue;         // �������
    locker m_queuelocker;               // ����������еĻ�����
    sem m_queuestat;                    // �Ƿ���������Ҫ����
    bool m_stop;                        // �Ƿ�����߳�
    connection_pool* m_connPool;        // ���ݿ�
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

    // ��������������Ҫ���ź�����һ
    m_queuestat.post();
}

template<class T>
void* threadpool<T>::worker(void* arg){
    threadpool* pool = (threadpool* )arg;
    pool->run();
    return pool;
}

// ! ���m_queuestat�������ź�������ʾ����ĸ�����Ϊʲô�����л�Ҫ�Թ������н����ж���
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