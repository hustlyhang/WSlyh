#include "ThreadPool.h"

CThreadPool::CThreadPool(int _tasksize, int _threadssize) {
    // ��ʼ���߳���������������С
    tasksize = _tasksize;
    threadssize = _threadssize;
    // ��ʼ�����������ͻ�����
    pthread_mutex_init(&m_mutex, NULL);
    pthread_cond_init(&m_cond, NULL);
    
}