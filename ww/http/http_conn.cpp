#include "http_conn.h"
#include <map>
#include "../lock/locker.h"
#include "../log/log.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <string.h>

// #define connfdET
#define connfdLT

// #define listenfdET
#define listenfdLT

// ����http��Ӧ��һЩ״̬��Ϣ
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

// �������������������ʱ����������վ��Ŀ¼�����http��Ӧ��ʽ������߷��ʵ��ļ���������ȫΪ��
const char *doc_root = "/home/ubuntu/WSlyh/ww/root";

// �����е��û������������map
std::map<string, string> users;
locker m_lock;

// �����ļ�������Ϊ������
int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

// ���ں�ʱ���ע����¼���ETģʽ��ѡ����EPOLLONESHOT
void addfd(int epollfd, int fd, bool one_shot)
{
    epoll_event event;
    event.data.fd = fd;
#ifdef connfdET
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
#endif

#ifdef connfdLT
    event.events = EPOLLIN | EPOLLRDHUP;
#endif 

#ifdef listenfdET
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
#endif

#ifdef listenfdLT
    event.events = EPOLLIN | EPOLLRDHUP;
#endif

    if (one_shot)
        event.events |= EPOLLONESHOT;
    
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);

}

// ���ں�ʱ�����ɾ��������
void removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

// ���¼�����ΪEPOLLONESHOT
void modfd(int epollfd, int fd, int ev)
{
    epoll_event event;
    event.data.fd = fd;

#ifdef connfdET
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
#endif

#ifdef connfdLT
    event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
#endif

    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

// ��ʼ����̬��Ա����
int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;

// �ر����ӣ��ر�һ�����ӣ��ͷ�������һ
void http_conn::close_conn(bool real_close)
{
    if (real_close && m_sockfd)
    {
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--;
    }
}

// ��ʼ�����ӣ��ⲿ���ó�ʼ���׽��ֵ�ַ
void http_conn::init(int sockfd, const sockaddr_in &addr)
{
    m_sockfd = sockfd;
    m_address = addr;
    addfd(m_epollfd, sockfd, true);
    m_user_count++;
    init();
}

// ��ʼ���½��յ�����
// check_stateĬ��Ϊ����������״̬
void http_conn::init()
{
    mysql = NULL;
    bytes_to_send = 0;
    bytes_have_send = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    cgi = 0;
    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
}


// ��״̬�������ڷ�����һ������
// ����ֵΪ�еĶ�ȡ״̬����LINE_OK, LINE_BAD, LINE_OPEN
http_conn::LINE_STATUS http_conn::parse_line()
{
    char temp;
    for (; m_checked_idx < m_read_idx; ++m_checked_idx)
    {
        temp = m_read_buf[m_checked_idx];
        if (temp == '\r')   // �س���
        {
            if ((m_checked_idx + 1) == m_read_idx) return LINE_OPEN;
            else if (m_read_buf[m_checked_idx + 1] == '\n')
            {
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if (temp == '\n')
        {
            if (m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r')
            {
                m_read_buf[m_checked_idx - 1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

// �൱�ڽ����ݿ��е����ݱ��浽map��
void http_conn::initmysql_result(connection_pool *connPool)
{
    // �ȴ����ӳ���ȡһ������
    MYSQL *mysql = NULL;
    connectionRAII mysqlcon(&mysql, connPool);

    // ��user���м���username��passwd���ݣ������������
    if (mysql_query(mysql, "SELECT username, passwd FROM user"))
        LOG_ERROR("SELECT error: %s", mysql_error(mysql));

    // �ӱ��м��������Ľ����
    MYSQL_RES *result = mysql_store_result(mysql);

    // ���ؽ�����е�����
    int num_fields = mysql_num_fields(result);

    // ���������ֶνṹ������
    MYSQL_FIELD *fields = mysql_fetch_fields(result);

    // �ӽ�����л�ȡ��һ�У�����Ӧ���û������������map��
    while (MYSQL_ROW row = mysql_fetch_row(result)) 
        users[string(row[0])] = string(row[1]);

}



