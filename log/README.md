每次测试都printf，不太方便，准备实现一个日志类，记录日志
因为主要是多线程，所以需要考虑线程安全，而且防止日志影响速度，实现同步和异步两种写入方式


杂：
    ✔int pthread_once(pthread_once_t *once_control, void (*init_routine) (void));
    此函数使用初值为PTHREAD_ONCE_INIT的once_control 变量保证init_routine()函数在本进程执行程序中仅执行一次

    ✔localtime_r 是可重入函数，线程安全

    ✔感觉可以直接用单例模式

    ✔在宏中使用 do {}while(0) 可以使每次使用宏效果都是相同的，不必担心在调用的位置使用了多少括号和分号

    ✔宏里面一行写不下，使用\来换行，但是后面不能跟空格

    ✔__VA_ARGS__宏用来接受不定数量的参数

        #define eprintf(...) fprintf (stderr, __VA_ARGS__)
        eprintf ("%s:%d: ", input_file, lineno)
        ==>  fprintf (stderr, "%s:%d: ", input_file, lineno)

    ✔当__VA_ARGS__宏前面##时，可以省略参数输入。
        #define eprintf(format, ...) fprintf (stderr, format, ##__VA_ARGS__)
        eprintf ("success!\n")
        ==> fprintf(stderr, "success!\n");
    ✔size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);


优化：
    每次记录日志都会去获取时间，当线程数量很多的时候，这会影响到性能，想想怎么优化
