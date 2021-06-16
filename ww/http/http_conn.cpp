#include "http_conn.h"
#include <map>
#include "../lock/locker.h"
#include "../log/log.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/mman.h>

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

// ��վ��Ŀ¼���ļ����ڴ���������Դ����ת��html�ļ�
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
// LINE_OPEN ��ʾ��δ������ɣ�LINE_BADΪ���У� LINE_OK��ʾ�д������
// m_read_idx ��ʾ�� m_read_buf �е�λ�ã� m_checked_idx ��ʾ�Ѿ������˵�λ��
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

// ! Ϊʲô����0Ҫ����false
// ѭ����ȡ�ͻ����ݣ�ֱ�������ݿɶ���Է��ر�����
// ������ET����ģʽ�£���Ҫһ���Խ����ݶ���
bool http_conn::read_once()
{
    if (m_read_idx >= READ_BUFFER_SIZE) return false;
    int bytes_read = 0;     // ��¼��ȡ�˶�������
#ifdef connfdLT
    bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
    m_read_idx += bytes_read;
    if (bytes_read <= 0) return false;
    return true;
#endif

#ifdef connfdET
    while (true)
    {
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        if (bytes_read == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            return false;
        }
        else if (bytes_read == 0)
        {
            return false;
        }
        m_read_idx += bytes_read;
    }
    return true;
#endif
}

// ����http�����У�������󷽷���Ŀ��url��http�汾��

http_conn::HTTP_CODE http_conn::parse_request_line(char *text)
{
    // ��ȡ����
    // ��HTTP�����У�����������˵����������,Ҫ���ʵ���Դ�Լ���ʹ�õ�HTTP�汾�����и�������֮��ͨ��\t��ո�ָ���
    // �����������Ⱥ��пո��\t��һ�ַ���λ�ò�����
    m_url = strpbrk(text, " \t");
    // ���û�пո��\t�����ĸ�ʽ����
    if (!m_url) return BAD_REQUEST;
    // ! ΪʲôҪ++
    // ����λ�ø�Ϊ\0�����ڽ�ǰ������ȡ��
    *(m_url++) = '\0';
    // ȡ�����ݣ���ͨ����GET��POST�Ƚϣ���ȷ������ʽ
    char *method = text;
    if (strcasecmp(method, "GET") == 0) m_method = GET;
    else if (strcasecmp(method, "POST") == 0) 
    {
        m_method = POST;
        cgi = 1;
    }
    else return BAD_REQUEST;
    // m_url��ʱ�����˵�һ���ո��\t�ַ�������֪��֮���Ƿ���
    // ��m_url���ƫ�ƣ�ͨ�����ң����������ո��\t�ַ���ָ��������Դ�ĵ�һ���ַ�
    m_url += strspn(m_url, " \t");     // �����ո�
    // ʹ�����ж�����ʽ����ͬ�߼����ж�HTTP�汾��
    m_version = strpbrk(m_url, " \t");
    if (!m_version) return BAD_REQUEST;
    *m_version++ = '\0';
    m_version += strspn(m_version, " \t");
    // ��֧��HTTP/1.1
    if (strcasecmp(m_version, "HTTP/1.1") != 0) return BAD_REQUEST;
    if (strncasecmp(m_url, "http://", 7) == 0)
    {
        m_url += 7;
        m_url = strchr(m_url, '/');
    }
    if (strncasecmp(m_url, "https://", 8) == 0)
    {
        m_url += 8;
        m_url = strchr(m_url, '/');
    }

    // һ��Ĳ�������������ַ��ţ�ֱ���ǵ�����/��/�����������Դ
    if (!m_url || m_url[0] != '/') return BAD_REQUEST;

    // ��url Ϊ'/'ʱ����ʾ�жϽ���
    if (strlen(m_url) == 1)
        strcat(m_url, "judge.html");

    // �����д�����ϣ�����״̬��ת�ƴ�������ͷ
    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;

}
// ����http�����һ��ͷ����Ϣ
http_conn::HTTP_CODE http_conn::parse_headers(char *text)
{
    if (text[0] == '\0')
    {
        if (m_content_length != 0)
        {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    else if (strncasecmp(text, "Connection:", 11) == 0)
    {
        text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0)
        {
            m_linger = true;
        }
    }
    else if (strncasecmp(text, "Content-length:", 15) == 0)
    {
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    }
    else if (strncasecmp(text, "Host:", 5) == 0)
    {
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }
    else
    {
        //printf("oop!unknow header: %s\n",text);
        LOG_INFO("oop!unknow header: %s", text);
        Log::get_instance()->flush();
    }
    return NO_REQUEST;
}

//�ж�http�����Ƿ���������
http_conn::HTTP_CODE http_conn::parse_content(char *text)
{
    //�ж�buffer���Ƿ��ȡ����Ϣ��
    if (m_read_idx >= (m_content_length + m_checked_idx))
    {
        text[m_content_length] = '\0';
        //POST���������Ϊ������û���������
        m_string = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::process_read()
{
    // ��ʼ����״̬��״̬��HTTP����������
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char *text = 0;
    // ! ���parse_line()���ص���line_open��
    while ((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = parse_line()) == LINE_OK))
    {
        text = get_line();
        // m_start_line��ÿһ����������m_read_buf�е���ʼλ��
        // m_checked_idx��ʾ��״̬����m_read_buf�ж�ȡ��λ��
        m_start_line = m_checked_idx;
        LOG_INFO("%s", text);
        Log::get_instance()->flush();
        // ��״̬��������״̬ת���߼�
        switch (m_check_state)
        {
        case CHECK_STATE_REQUESTLINE:
        {
            ret = parse_request_line(text);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            break;
        }
        case CHECK_STATE_HEADER:
        {
            ret = parse_headers(text);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            // ��������GET�������ת��������Ӧ����
            else if (ret == GET_REQUEST)
            {
                return do_request();
            }
            break;
        }
        case CHECK_STATE_CONTENT:
        {
            ret = parse_content(text);
            // ��������POST�������ת��������Ӧ����
            if (ret == GET_REQUEST)
                return do_request();
            // ��������Ϣ�弴��ɱ��Ľ����������ٴν���ѭ��������line_status
            line_status = LINE_OPEN;
            break;
        }
        default:
            return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}

/*
    m_url Ϊ���������н��������������ַ
*/
http_conn::HTTP_CODE http_conn::do_request()
{
    // ����ʼ����m_real_file��ֵΪ��վ��Ŀ¼
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);
    //printf("m_url:%s\n", m_url);
    const char *p = strrchr(m_url, '/');

    //����cgi
    if (cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3'))
    {

        //���ݱ�־�ж��ǵ�¼��⻹��ע����
        char flag = m_url[1];

        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/");
        strcat(m_url_real, m_url + 2);
        strncpy(m_real_file + len, m_url_real, FILENAME_LEN - len - 1);
        free(m_url_real);

        //���û�����������ȡ����
        //user=123&passwd=123
        char name[100], password[100];
        int i;
        for (i = 5; m_string[i] != '&'; ++i)
            name[i - 5] = m_string[i];
        name[i - 5] = '\0';

        int j = 0;
        for (i = i + 10; m_string[i] != '\0'; ++i, ++j)
            password[j] = m_string[i];
        password[j] = '\0';

        //ͬ���̵߳�¼У��
        if (*(p + 1) == '3')
        {
            //�����ע�ᣬ�ȼ�����ݿ����Ƿ���������
            //û�������ģ�������������
            char *sql_insert = (char *)malloc(sizeof(char) * 200);
            strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
            strcat(sql_insert, "'");
            strcat(sql_insert, name);
            strcat(sql_insert, "', '");
            strcat(sql_insert, password);
            strcat(sql_insert, "')");

            if (users.find(name) == users.end())
            {

                m_lock.lock();
                int res = mysql_query(mysql, sql_insert);
                users.insert(pair<string, string>(name, password));
                m_lock.unlock();

                if (!res)
                    strcpy(m_url, "/log.html");
                else
                    strcpy(m_url, "/registerError.html");
            }
            else
                strcpy(m_url, "/registerError.html");
        }
        //����ǵ�¼��ֱ���ж�
        //���������������û����������ڱ��п��Բ��ҵ�������1�����򷵻�0
        else if (*(p + 1) == '2')
        {
            if (users.find(name) != users.end() && users[name] == password)
                strcpy(m_url, "/welcome.html");
            else
                strcpy(m_url, "/logError.html");
        }
    }
    // ���������ԴΪ/0����ʾ��תע�����
    if (*(p + 1) == '0')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/register.html");
        // ����վĿ¼��/register.html����ƴ�ӣ����µ�m_real_file��
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }
    // ���������ԴΪ/1����ʾ��ת��¼����
    else if (*(p + 1) == '1')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/log.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }

    else if (*(p + 1) == '5')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/picture.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '6')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/video.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '7')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/fans.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else
        // ������Ͼ������ϣ������ǵ�¼��ע�ᣬֱ�ӽ�url����վĿ¼ƴ��
        // ����������welcome���棬����������ϵ�һ��ͼƬ
        strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);

    // ͨ��stat��ȡ������Դ�ļ���Ϣ���ɹ�����Ϣ���µ�m_file_stat�ṹ��
    // ʧ�ܷ���NO_RESOURCE״̬����ʾ��Դ������
    if (stat(m_real_file, &m_file_stat) < 0)
        return NO_RESOURCE;
    // �ж��ļ���Ȩ�ޣ��Ƿ�ɶ������ɶ��򷵻�FORBIDDEN_REQUEST״̬
    if (!(m_file_stat.st_mode & S_IROTH))
        return FORBIDDEN_REQUEST;
    // �ж��ļ����ͣ������Ŀ¼���򷵻�BAD_REQUEST����ʾ����������
    if (S_ISDIR(m_file_stat.st_mode))
        return BAD_REQUEST;
    // ��ֻ����ʽ��ȡ�ļ���������ͨ��mmap�����ļ�ӳ�䵽�ڴ���
    int fd = open(m_real_file, O_RDONLY);
    m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    // ��ʾ�����ļ����ڣ��ҿ��Է���
    return FILE_REQUEST;
}

void http_conn::unmap()
{
    if (m_file_address)
    {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}

bool http_conn::write()
{
    int temp = 0;

    int newadd = 0;

    // ��Ҫ���͵����ݳ���Ϊ0,��ʾ��Ӧ����Ϊ�գ�һ�㲻������������
    if (bytes_to_send == 0)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        init();
        return true;
    }

    while (1)
    {   
        // ����Ӧ���ĵ�״̬�С���Ϣͷ�����к���Ӧ���ķ��͸��������
        temp = writev(m_sockfd, m_iv, m_iv_count);

        if (temp > 0) 
        {
            // �����ѷ����ֽ�
            bytes_have_send += temp;
            // ƫ���ļ�iovec��ָ��
            newadd = bytes_have_send - m_write_idx;
        }
        if (temp <= -1) 
        {
            if (errno == EAGAIN)
            {
                // �жϵ�һ������д��û
                if (bytes_have_send >= m_iv[0].iov_len)
                {
                    // ���д���ˣ��ͽ���һ������������Ϊ0
                    m_iv[0].iov_len = 0;
                    // ͬʱ���µڶ��������е�λ��
                    m_iv[1].iov_base = m_file_address + newadd;
                    m_iv[1].iov_len = bytes_to_send;
                }
                else 
                {
                    // û��д�꣬���µ�һ��λ�õ�ƫ��ֵ
                    m_iv[0].iov_base = m_write_buf + bytes_to_send;
                    m_iv[0].iov_len -= bytes_to_send;
                }
                // ����ע��д�¼�
                modfd(m_epollfd, m_sockfd, EPOLLOUT);
                return true;
            }
            else
            {
                unmap();
                return false;
            }
        }

        // �����ѷ����ֽ���
        bytes_to_send -= temp;
    

        if (bytes_to_send <= 0)
        {
            unmap();
            modfd(m_epollfd, m_sockfd, EPOLLIN);

            if (m_linger)
            {
                init();
                return true;
            }
            else
            {
                return false;
            }
        }
    }
}

bool http_conn::add_response(const char *format, ...)
{
    if (m_write_idx >= WRITE_BUFFER_SIZE)
        return false;
    
    // ����ɱ�����б�
    va_list arg_list;

    // ������arg_list��ʼ��Ϊ�������
    va_start(arg_list, format);
    // ������format�ӿɱ�����б�д�뻺����д������д�����ݵĳ���
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);
    // ���д������ݳ��ȳ���������ʣ��ռ䣬�򱨴�
    if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx))
    {
        va_end(arg_list);
        return false;
    }
    // ����m_write_idxλ��
    m_write_idx += len;
    va_end(arg_list);
    LOG_INFO("request:%s", m_write_buf);
    Log::get_instance()->flush();
    return true;
}
// ���״̬��
bool http_conn::add_status_line(int status, const char *title)
{
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}
// �����Ϣ��ͷ�����������ı����ȡ�����״̬�Ϳ���
bool http_conn::add_headers(int content_len)
{
    add_content_length(content_len);
    add_linger();
    add_blank_line();
}
bool http_conn::add_content_length(int content_len)
{
    return add_response("Content-Length:%d\r\n", content_len);
}
bool http_conn::add_content_type()
{
    return add_response("Content-Type:%s\r\n", "text/html");
}
// �������״̬��֪ͨ��������Ǳ������ӻ��ǹر�
bool http_conn::add_linger()
{
    return add_response("Connection:%s\r\n", (m_linger == true) ? "keep-alive" : "close");
}
// ��ӿ���
bool http_conn::add_blank_line()
{
    return add_response("%s", "\r\n");
}
// ����ı�content
bool http_conn::add_content(const char *content)
{
    return add_response("%s", content);
}

bool http_conn::process_write(HTTP_CODE ret)
{
    switch (ret)
    {
    // �ڲ�����500    
    case INTERNAL_ERROR:
    {
        add_status_line(500, error_500_title);

        add_headers(strlen(error_500_form));

        if (!add_content(error_500_form))
            return false;
        break;
    }
    // �����﷨����404
    case BAD_REQUEST:
    {
        add_status_line(404, error_404_title);
        add_headers(strlen(error_404_form));
        if (!add_content(error_404_form))
            return false;
        break;
    }
    // ��Դû�з���Ȩ�ޣ�403
    case FORBIDDEN_REQUEST:
    {
        add_status_line(403, error_403_title);
        add_headers(strlen(error_403_form));
        if (!add_content(error_403_form))
            return false;
        break;
    }
    case FILE_REQUEST:
    {
        add_status_line(200, ok_200_title);
        // ����������Դ����
        if (m_file_stat.st_size != 0)
        {
            add_headers(m_file_stat.st_size);
            // ��һ��iovecָ��ָ����Ӧ���Ļ�����������ָ��m_write_idx
            m_iv[0].iov_base = m_write_buf;
            m_iv[0].iov_len = m_write_idx;
            // �ڶ���iovecָ��ָ��mmap���ص��ļ�ָ�룬����ָ���ļ���С
            m_iv[1].iov_base = m_file_address;
            m_iv[1].iov_len = m_file_stat.st_size;
            m_iv_count = 2;
            // ���͵�ȫ������Ϊ��Ӧ����ͷ����Ϣ���ļ���С
            bytes_to_send = m_write_idx + m_file_stat.st_size;
            return true;
        }
        else
        {
            // ����������Դ��СΪ0���򷵻ؿհ�html�ļ�
            const char *ok_string = "<html><body></body></html>";
            add_headers(strlen(ok_string));
            if (!add_content(ok_string))
                return false;
        }
    }
    default:
        return false;
    }
    // ��FILE_REQUEST״̬�⣬����״ֻ̬����һ��iovec��ָ����Ӧ���Ļ�����
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    bytes_to_send = m_write_idx;
    return true;
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


void http_conn::process()
{
    HTTP_CODE read_ret = process_read();
    if (read_ret == NO_REQUEST)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        return;
    }
    bool write_ret = process_write(read_ret);
    if (!write_ret)
    {
        close_conn();
    }
    modfd(m_epollfd, m_sockfd, EPOLLOUT);
}

