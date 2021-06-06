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

    // 可选择的参数有日志文件、日志缓冲区大小、最大行数以及最长日志条队列
    bool init(const char* file_name, int log_buf_size = 8192, int split_lines = 5e6, int max_queue_size = 0);

    void write_log(int level, const char* format, ...);

    void flush(void);

private:
    Log();
    virtual ~Log();

    void* async_write_log();


private:
    char dir_name[128];                 // 路径名
    char log_name[128];                 // log文件名
    int m_split_lines;                  // 日志最大行数
    int m_log_buf_size;                 // 日志缓冲区大小
    long long m_count;                  // 日志行数记录
    int m_today;                        // 记录日志天数，按天数进行分类
    FILE* m_fp;                         // 打开log的文件指针
    char* m_buf;
    block_queue<string>* m_log_queue;   // 阻塞队列
    bool m_is_async;                    // 是否同步标志位
    locker m_mutex;
};


/*
    ##运算符可以用于宏函数的替换部分。
    这个运算符把两个语言符号组合成单个语言符号，
    为宏扩展提供了一种连接实际变元的手段
    ##__VA_ARGS__ 宏前面加上##的作用在于，当可变参数的个数为0时，
    这里的##起到把前面多余的","去掉的作用,否则会编译出错
*/

#define LOG_DEBUG(format, ...) Log::get_instance()->write_log(0, format, ##__VA_ARGS__)
#define LOG_INFO(format, ...) Log::get_instance()->write_log(1, format, ##__VA_ARGS__)
#define LOG_WARN(format, ...) Log::get_instance()->write_log(2, format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) Log::get_instance()->write_log(3, format, ##__VA_ARGS__)


#endif // !LOG_H