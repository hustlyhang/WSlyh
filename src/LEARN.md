**知识点：**  
-----------------------------------------
- int pthread_once(pthread_once_t *once_control, void (*init_routine) (void));
    此函数使用初值为PTHREAD_ONCE_INIT的once_control 变量保证init_routine()函数在本进程执行程序中仅执行一次

- localtime_r 是可重入函数，线程安全

- 感觉可以直接用单例模式

- 在宏中使用 do {}while(0) 可以使每次使用宏效果都是相同的，不必担心在调用的位置使用了多少括号和分号

- 宏里面一行写不下，使用\来换行，但是后面不能跟空格

- __VA_ARGS__宏用来接受不定数量的参数

        #define eprintf(...) fprintf (stderr, __VA_ARGS__)
        eprintf ("%s:%d: ", input_file, lineno)
        ==>  fprintf (stderr, "%s:%d: ", input_file, lineno)

- 当__VA_ARGS__宏前面##时，可以省略参数输入。  
      ```#define eprintf(format, ...) fprintf (stderr, format, ##__VA_ARGS__)
        eprintf ("success!\n")  
        ==> fprintf(stderr, "success!\n");```
- size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);

- mkdir()
- fflush(FILE* stream)  ```#include<stdio.h>```
  - 清空文件缓冲区，或者标准输入输出缓冲区,如果文件是打开的，就将缓冲区内容写入文件

- strerror()
- rename()
- __FUNCTION__  
    - __ __FUNCTION__ __：返回当前代码所在函数的名字
    - __ __FILE__ __： 就是当前源代码文件名
    - __ __LINE__ __： 就是当前源代码的行号
- ```ssize_t readlink(const char *path, char *buf, size_t bufsiz);```
  - #include<unistd.h>
  - 将参数path的符号链接内容存储到参数buf所指的内存空间
  - path为```/proc/self/exe```代表当前程序的绝对路径，这样读取会获得当前程序的绝对路径

- realpath 
  -   ```char *realpath(const char *path, char *resolved_path)```
  -    realpath()用来将参数path所指的相对路径转换成绝对路径后存于参数resolved_path所指的字符串数组或指针中

共享内存相关函数
  --------------------------------
- **ftok**   [ftok](https://blog.csdn.net/u013485792/article/details/50764224)
  - ```
    #include <sys/types.h>
    #include <sys/ipc.h>
    key_t ftok(const char *pathname, int proj_id);
    ```
    将pathname和proj_id转为系统IPC key
    获取唯一ID，用于生成共享内存

- **shmget**   [shmget](https://www.cnblogs.com/52php/p/5861372.html)
  - ```
    #include <sys/ipc.h>
    #include <sys/shm.h>
    int shmget(key_t key, size_t size, int shmflg);
    ```
    分配一段共享内存，key有效地为共享内存段命名，shmget()函数成功时返回一个与key相关的共享内存标识符（非负整数），用于后续的共享内存函数。调用失败返回-1.
    第三个参数和open的mode相似，描述这块内存段的权限

- **shmat**   [shmat](https://www.cnblogs.com/52php/p/5861372.html)
  - ```
     #include <sys/types.h>
     #include <sys/shm.h>
     void *shmat(int shmid, const void *shmaddr, int shmflg);
    ```
    第一次创建完共享内存时，它还不能被任何进程访问，shmat()函数的作用就是用来启动对该共享内存的访问，并把共享内存连接到当前进程的地址空间。  
    第一个参数，shm_id是由shmget()函数返回的共享内存标识。  
    第二个参数，shm_addr指定共享内存连接到当前进程中的地址位置，通常为空，表示让系统来选择共享内存的地址。  
    第三个参数，shm_flg是一组标志位，通常为0。  
    调用成功时返回一个指向共享内存第一个字节的指针，如果调用失败返回-1.  

- **shmdt**   [shmdt](https://www.cnblogs.com/52php/p/5861372.html)
  - ```
    #include <sys/types.h>
    #include <sys/shm.h>
    int shmdt(const void *shmaddr);
    ```
    该函数用于将共享内存从当前进程中分离。注意，将共享内存分离并不是删除它，只是使该共享内存对当前进程不再可用。    
    参数shmaddr是shmat()函数返回的地址指针，调用成功时返回0，失败时返回-1. 

- **shmctl**   [shmctl](https://www.cnblogs.com/52php/p/5861372.html)
  - ```
    #include <sys/ipc.h>
    #include <sys/shm.h>
    int shmctl(int shmid, int cmd, struct shmid_ds *buf);

    struct shmid_ds {
        uid_t shm_perm.uid;
        uid_t shm_perm.gid;
        mode_t shm_perm.mode;
    };
    ```
    与信号量的semctl()函数一样，用来控制共享内存  
    第一个参数，shm_id是shmget()函数返回的共享内存标识符。  
    第二个参数，command是要采取的操作，它可以取下面的三个值 ：  
    1. IPC_STAT：把shmid_ds结构中的数据设置为共享内存的当前关联值，即用共享内存的当前关联值覆盖shmid_ds的值。  
    1. IPC_SET：如果进程有足够的权限，就把共享内存的当前关联值设置为shmid_ds结构中给出的值  
    1. IPC_RMID：删除共享内存段

    第三个参数，buf是一个结构指针，它指向共享内存模式和访问权限的结构。


- [strftime](https://www.runoob.com/cprogramming/c-function-strftime.html)
  - ```C++
    size_t strftime(char *str, size_t maxsize, const char *format, const struct tm *timeptr)
    ```
    根据 format 中定义的格式化规则，格式化结构 timeptr 表示的时间，并把它存储在 str 中。