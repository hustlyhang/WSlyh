#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <iostream>
#include <string>
#include <sys/time.h>
#include "block_queue.h"
#include <stdarg.h>

using namespace std;

class Log{

public:
    static Log* get_instance(){
        static Log instance;
        return &instance;
    }
    static void* flush_log_thread(void* args){
        Log::get_instance()->async_write_log();
    }

    // ��ѡ��Ĳ�������־�ļ�����־��������С����������Լ����־������
    bool init(const char* file_name, int log_buf_size = 8192, int split_lines = 5e6, int max_queue_size = 0);

    void write_log(int level, const char* format, ...);

    void flush(void);

private:
    Log();
    virtual ~Log();

    void* async_write_log();


private:
    char dir_name[128];                 // ·����
    char log_name[128];                 // log�ļ���
    int m_split_lines;                  // ��־�������
    int m_log_buf_size;                 // ��־��������С
    long long m_count;                  // ��־������¼
    int m_today;                        // ��¼��־���������������з���
    FILE* m_fp;                         // ��log���ļ�ָ��
    char* m_buf;
    block_queue<string>* m_log_queue;   // ��������
    bool m_is_async;                    // �Ƿ�ͬ����־λ
    locker m_mutex;
};


/*
    ##������������ں꺯�����滻���֡�
    �����������������Է�����ϳɵ������Է��ţ�
    Ϊ����չ�ṩ��һ������ʵ�ʱ�Ԫ���ֶ�
    ##__VA_ARGS__ ��ǰ�����##���������ڣ����ɱ�����ĸ���Ϊ0ʱ��
    �����##�𵽰�ǰ������","ȥ��������,�����������
*/

#define LOG_DEBUG(format, ...) Log::get_instance()->write_log(0, format, ##__VA_ARGS__)
#define LOG_INFO(format, ...) Log::get_instance()->write_log(1, format, ##__VA_ARGS__)
#define LOG_WARN(format, ...) Log::get_instance()->write_log(2, format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) Log::get_instance()->write_log(3, format, ##__VA_ARGS__)


#endif // !LOG_H