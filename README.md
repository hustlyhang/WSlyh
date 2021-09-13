# WSlyh
- **主要功能**
    - 使用 线程池 + 非阻塞socket + epoll(ET模式) + 事件处理(Proactor) 的并发模型
    - 实现基于小根堆的计时器，可以高效剔除长时间非活跃连接
    - 异步日志系统，采用循环双向链表结构，便于扩展同时提高写入量
    - 利用有限状态机处理http请求，解析数据
    - 使用Webbench测试，可以实现上万的并发连接

- **文件目录**
    - ./http ：http连接处理类；基于小根堆的定时器类实现；
    - ./lock ： 封装了互斥锁、型号量、条件变量；
    - ./log ： 异步日志系统，其中./log/shmtool.cpp 文件是为了获取程序异常退出时剩余未写入日志文件的日志记录；异步日志和同步日志的测试在./log/test文件中；
    - ./threadpool：线程池
    - ./unit_test：单元测试
    - ./src：资源文件


- **快速运行**
    - 服务器测试环境  
        - Ubuntu版本20.04
    - 浏览器
        - Windows、Linux均可
        - Chrome
        - FireFox
        - todo...
    - 在当前目录下
        ```C++;
        make;
        ./server 9999 3
        ```
        其中9999是端口号，3是日志等级

        程序异常崩溃后，刷出共享内存中的日志：
        切到./log目录下
        ```C++
        make;
        ./Mtool -f ../server -d
        ```
        -f 后面跟运行的软件路径，-d为是否读取后删除


- **模块介绍**
    - [http处理](https://github.com/hustlyhang/WSlyh/blob/master/http/README.md)
    - [小根堆定时器](https://github.com/hustlyhang/WSlyh/blob/master/http/README.md)
    - [异步日志系统](https://github.com/hustlyhang/WSlyh/blob/master/log/README.md)


- **压力测试**
    - ![pic](https://github.com/hustlyhang/WSlyh/blob/master/src/webbenchtest.png)

- 遇到的一些问题  
    - [problem](https://github.com/hustlyhang/WSlyh/blob/master/src/PROBLEM.md)

- 项目框架
    - todo

- TODO
    - 日志系统捕获结束信号，内存中剩余的日志写完
    - 加入kv存储引擎，实现用户注册功能
    - 优化代码结构
    - 定时器类中需要定时删除已经被标记删除的节点，长时间运行会导致内存消耗过高
