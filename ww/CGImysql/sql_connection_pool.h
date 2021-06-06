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
    MYSQL* GetConnection();                 // ��ȡ���ݿ�����
    bool ReleaseConnection(MYSQL* conn);    // �ͷ�����
    int GetFreeConn();                      // ��ȡ����
    void DestroyPool();                     // ������������

    // ����ģʽ, �ֲ���̬����������ʽ
    static connection_pool* GetInstance();

    void init(string url, string User, string PassWord, string DataBaseName, int Port, unsigned int MaxConn);
    
private:
    connection_pool();
    ~connection_pool();

private:
    unsigned int MaxConn;                   // ����������
    unsigned int CurConn;                   // ��ǰ��ʹ�õ�������
    unsigned int FreeConn;                  // ��ǰ���е�������

    locker lock;
    list<MYSQL* > connList;                 // ���ӳ�
    sem reserve;

    string url;                             // ������ַ
    string Port;                            // ���ݿ�˿ں�
    string User;                            // ��¼���ݿ��û���
    string PassWord;                        // ��¼���ݿ�����
    string DatabaseName;                    // ʹ�����ݿ���
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
