#pragma once
// �̳߳�
#include <pthread.h>

// ����ṹ�壬����ִ�к�������Ӧ�Ĳ���
typedef struct{
    void (*func)(void *);
    void *argument;
}STask;


class CThreadPool{
  private:
    STask*              m_arryTasks;                // ��������
    pthread_t*          m_arryThreads;              // �߳�����
    pthread_cond_t      m_cond;                     // ��������
    pthread_mutex_t     m_mutex;                    // ������
    int                 head, tail;                 // �������������洢���������е��������
    int                 started;                    // ��¼������������߳���
    int                 tasksize,threadssize;       // ����������߳�����Ĵ�С
  public:
    CThreadPool(int _tasksize, int _threadssize);
    ~CThreadPool();
    
    void AddTask(STask m_sTask);    // �������
    void ThreadBegin();             // �߳�ִ�к���
    
    
};

