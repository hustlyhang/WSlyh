# 异步日志系统
- 简介
  - 数据结构：循环双向链表；
  - 效率高：建立日志缓冲区、优化UTC日志时间生成策略；
  - 易拓展：基于双向循环链表构建日志缓冲区，其中每个节点是一个小的日志缓冲区


- 工作流程
  - CRingLog类是由若干个CellBuffer组成的循环双向链表，日志都落于每个CellBuffer中，CellBuffer有两种状态
    - ```FULL```：表示当前CellBuffer已满，正在或即将被持久化到磁盘
    - ```FREE```：表示可以写入日志
  - CRingLog类有两个指针：
    - ```m_pCurBuf```：生产者向此指针指向的CellBuffer中写入记录，写满后指针向后前移动一个CellBuffer，可以被多个生产者持有；
    - ```m_pPrstBuf```：消费者持有此指针，该线程会不断循环持久化当前状态为FULL的CellBuffer，然后将此CellBuffer状态改为```FREE```，然后向前移动，此指针只被一个消费者线程持有；
    - 初始状态：所有的CellBuffer状态都为```FREE```，且```m_pCurBuf```和```m_pPrstBuf```都指向同一个CellBuffer，通过互斥锁来保护临界资源；  
    ![pic]()
  - 消费者
    - 一直循环直到程序结束  
    1. 上锁，检查```m_pPrstBuf```状态：
        - ```FULL```：释放锁，go Step 4；
        - ```FREE```：超时等待条件变量cond(1s);
    2. 再次检查```m_pPrstBuf```状态：
        - ```FULL```：释放锁，go Step 4；
        - ```FREE```：
          - 无内容：释放锁， go Step 1；
          - 有内容：标记为```FULL```，同时让```m_pCurBuf```向前走一步；
    3. 释放锁（尽量使锁的范围小）
    4. 持久化```m_pPrstBuf```指向的CellBuffer
    5. 加锁，CellBuffer状态改为```FREE```，清空；```m_pPrstBuf```下移一步；
    6. 释放锁
  - 生产者
    1. 上锁，检查当前```m_pCurBuf```指向的CellBuffer状态： 
        - ```FREE```：
          - 剩余空间足以写入本次日志，则追加日志到CellBuffer；
          - 不足，修改状态为FULL，检查下一CellBuffer的状态：
            - ```FREE```：前进一位；
            - ```FULL```：说明```m_pPrstBuf == m_pCurBuf```，缓冲区无剩余空间，扩展一个CellBuffer，插入当前```m_pCurBuf```和其之后的CellBuffer之间的位置，然后```m_pCurBuf```前进一位；
        - ```FULL```：丢弃日志
    2. 释放锁，如果有CellBuffer状态从```FREE```转为```FULL```，则通知条件变量cond，唤醒等待的消费者线程；
  - 图解
    1. 初始状态
      ![pic](https://github.com/hustlyhang/WSlyh/blob/master/src/logflow1.png);
    2. 生产者写满一个CellBuffer
      ![pic](https://github.com/hustlyhang/WSlyh/blob/master/src/logflow2.png)
    3. 消费者持久化完成，重置CellBuffer1，```m_pPrstBuf```前进一位，发现指向的CellBuffer2未满，等待
      ![pic](https://github.com/hustlyhang/WSlyh/blob/master/src/logflow3.png)
    4. 超过一秒后 CellBuffer2 虽有日志，但依然未满：消费者将此CellBuffer2标记为```FULL```强行持久化，并将```m_pCurBuf```前进一位到CellBuffer3
      ![pic](https://github.com/hustlyhang/WSlyh/blob/master/src/logflow4.png)
    5. 消费者在 CellBuffer2 的持久化上延迟过大，结果生产者都写满整个链表，已经正在写第二次 CellBuffer1 了
      ![pic](https://github.com/hustlyhang/WSlyh/blob/master/src/logflow5.png)
    6. 生产者写满写 CellBuffer1 ，发现下一位 CellBuffer2 是```FULL```，则拓展换冲区，新增NewCellBuffer
      ![pic](https://github.com/hustlyhang/WSlyh/blob/master/src/logflow6.png)
    
- 测试
  1. 单线程写1千万条100KB记录对比：
    - ![pic](https://github.com/hustlyhang/WSlyh/blob/master/src/onthreadtest.png)
    - 可以看出性能差距接近9倍
  2. 多线程每个线程写1百万条100KB记录对比：线程数4
    - ![pic](https://github.com/hustlyhang/WSlyh/blob/master/src/multhreadtest.png)
    - 在多线程的情况异步日志系统为同步系统的7倍左右


**优化：**  

- 每次记录日志都会去获取时间，当线程数量很多的时候，这会影响到性能，优化：不用每次都更新时间，只有当时间超过一分钟时才刷新整个时间缓冲区。

