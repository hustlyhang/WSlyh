#include "sql_connection_pool.h"
#include <pthread.h>
#include <iostream>

using namespace std;

connection_pool::connection_pool(){
    this->CurConn = 0;
    this->FreeConn = 0;
}

connection_pool* connection_pool::GetInstance(){
    static connection_pool connPool;
    return &connPool; 
}

// �����ʼ��
void connection_pool::init(string url, string User, string PassWord, string DataBaseName, int Port, unsigned int MaxConn){
    this->url = url;
    this->User = User;
    this->PassWord = PassWord;
    this->DatabaseName = DatabaseName;
    this->Port = Port;

    // �������������ӣ�ͬʱ�����ӷ��뵽���ӳ��У����ӳ�Ϊ�ٽ�������Ҫ����
    lock.lock();
    for (int i = 0; i < MaxConn; ++i) {
        MYSQL* con = NULL;
        con = mysql_init(con);

        if (con == NULL) {
            cout<<"Error:"<<mysql_error(con)<<endl;
            exit(1);
        }
        
        con = mysql_real_connect(con, url.c_str(), User.c_str(), PassWord.c_str(), DatabaseName.c_str(), Port, NULL, 0);

        if (con == NULL) {
            cout<<"Error:"<<mysql_error(con)<<endl;
            exit(1);
        }
        connList.push_back(con);
        ++FreeConn;
    }

    // ��ʼ���ź���
    reserve = sem(FreeConn);

    this->MaxConn = FreeConn;

    lock.unlock();
}

// ��������ʱ�������ݿ����ӳ��з���һ���������ӣ�����ʹ�úͿ���������
MYSQL* connection_pool::GetConnection(){
    MYSQL* con = NULL;
    if (connList.size() == 0) return NULL;
    
    reserve.wait();

    lock.lock();
    con = connList.front();
    connList.pop_front();
    --FreeConn;
    ++CurConn;
    lock.unlock();

    return con;
}

// �ͷŵ�ǰ������
bool connection_pool::ReleaseConnection(MYSQL* con){
    if (con == NULL) return false;

    lock.lock();
    connList.push_back(con);
    ++FreeConn;
    --CurConn;
    lock.unlock();

    reserve.post();
    return true;
}

// �������ݿ����ӳ�
void connection_pool::DestroyPool(){

    lock.lock();
    if (connList.size()) 
        for (auto &c : connList) mysql_close(c);
    CurConn = 0;
    FreeConn = 0;
    connList.clear();
    lock.unlock();
}

// ��ǰ���е�������
int connection_pool::GetFreeConn(){
    return this->FreeConn;
}

connection_pool::~connection_pool(){
    DestroyPool();
}


connectionRAII::connectionRAII(MYSQL** SQL, connection_pool* connPool){
    *SQL = connPool->GetConnection();

    conRAII = *SQL;
    poolRAII = connPool;
}

connectionRAII::~connectionRAII(){
    poolRAII->DestroyPool();
}
