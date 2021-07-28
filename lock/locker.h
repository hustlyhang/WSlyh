#ifndef LOCKER_H
#define LOCKER_H

#include <pthread.h>
#include <mutex>
#include <semaphore.h>
#include <condition_variable>
#include <exception>


// 包装互斥锁
class CLocker {
public:
    CLocker(){
        if (pthread_mutex_init(&m_mutex, NULL) != 0) {
            throw std::exception();
        }
    }
    ~CLocker() {
        pthread_mutex_destroy(&m_mutex);
    }

    bool lock() {
        return pthread_mutex_lock(&m_mutex) == 0;
    }

    bool unlock() {
        return pthread_mutex_unlock(&m_mutex) == 0;
    }

    pthread_mutex_t* get() {
        return &m_mutex;
    }

private:
    pthread_mutex_t m_mutex;
};

// 包装信号量
class CSem {
public:
    CSem() {
        if (sem_init(&m_sem, 0, 0) != 0) {
            throw std::exception();
        }
    }
    CSem(int num) {
        if (sem_init(&m_sem, 0, num) != 0) {
            throw std::exception();
        }
    }
    ~CSem() {
        sem_destroy(&m_sem);
    }

    bool post() {
        return sem_post(&m_sem) == 0;
    }

    bool wait() {
        return sem_wait(&m_sem) == 0;
    }

private:
    sem_t m_sem;
};


// 包装条件变量
// 为什么在wait后面的函数中需要用局部变量保存值
class CCond {
public:
    CCond() {
        if (pthread_cond_init(&m_cond, NULL) != 0) throw std::exception();
    }

    ~CCond() {
        pthread_cond_destroy(&m_cond);
    }


    bool wait(pthread_mutex_t *mutex) {
        int ret = 0;
        ret = pthread_cond_wait(&m_cond, mutex);
        return ret == 0;
    }

    bool wait_time(pthread_mutex_t *mutex, struct timespec time) {
        int ret = 0;
        ret = pthread_cond_timedwait(&m_cond, mutex, &time);
        return ret == 0;
    }

    bool signal() {
        return pthread_cond_signal(&m_cond) == 0;
    }

    bool broadcast() {
        return pthread_cond_broadcast(&m_cond) == 0;
    }
private:
    pthread_cond_t m_cond;
};
#endif