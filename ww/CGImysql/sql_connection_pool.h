#ifndef _CONNECTION_POOL_
#define _CONNECTION_POOL_

#include "../lock/locker.h"
#include <stdio.h>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <string>
#include <list>

using namespace std;

class connection_pool{

public:
    MYSQL* GetConnection();                 // 获取数据库连接
    bool ReleaseConnection(MYSQL* conn);    // 释放连接
    int GetFreeConn();                      // 获取连接
    void DestroyPool();                     // 销毁所有连接

    // 单例模式, 局部静态变量，懒汉式
    static connection_pool* GetInstance();

    void init(string url, string User, string PassWord, string DataBaseName, int Port, unsigned int MaxConn);
    
private:
    connection_pool();
    ~connection_pool();

private:
    unsigned int MaxConn;                   // 最大的连接数
    unsigned int CurConn;                   // 当前已使用的连接数
    unsigned int FreeConn;                  // 当前空闲的连接数

    locker lock;
    list<MYSQL* > connList;                 // 连接池
    sem reserve;

    string url;                             // 主机地址
    string Port;                            // 数据库端口号
    string User;                            // 登录数据库用户名
    string PassWord;                        // 登录数据库密码
    string DatabaseName;                    // 使用数据库名
};

class connectionRAII{

public:
    connectionRAII(MYSQL** con, connection_pool* connPool);
    ~connectionRAII();
private:
    MYSQL* conRAII;
    connection_pool* poolRAII;
};
#endif // !_CONNECTION_POOL_
