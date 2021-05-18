#ifndef REQUESTDATA
#define REQUESTDATA
#include <string>
#include <unordered_map>


//? 描述请求数据中的状态，处理到哪一步了
const int STATE_PARSE_URI = 1;
const int STATE_PARSE_HEADERS = 2;
const int STATE_RECV_BODY = 3;
const int STATE_ANALYSIS = 4;
const int STATE_FINISH = 5;

const int MAX_BUFF = 4096;

// 有请求出现但是读不到数据,可能是Request Aborted,
// 或者来自网络的数据没有达到等原因,
// 对这样的请求尝试超过一定的次数就抛弃
const int AGAIN_MAX_TIMES = 200;

const int PARSE_URI_AGAIN = -1;
const int PARSE_URI_ERROR = -2;
const int PARSE_URI_SUCCESS = 0;

const int PARSE_HEADER_AGAIN = -1;
const int PARSE_HEADER_ERROR = -2;
const int PARSE_HEADER_SUCCESS = 0;

const int ANALYSIS_ERROR = -2;
const int ANALYSIS_SUCCESS = 0;

const int METHOD_POST = 1;
const int METHOD_GET = 2;
const int HTTP_10 = 1;
const int HTTP_11 = 2;

const int EPOLL_WAIT_TIME = 500;

//! 单例模式？
class MimeType
{
private:
    static pthread_mutex_t lock;
    static std::unordered_map<std::string, std::string> mime;
    MimeType();
    MimeType(const MimeType &m);
public:
    static std::string getMime(const std::string &suffix);
};

//? 请求头的状态
enum HeadersState
{
    h_start = 0,
    h_key,
    h_colon,
    h_spaces_after_colon,
    h_value,
    h_CR,
    h_LF,
    h_end_CR,
    h_end_LF
};

struct mytimer;
struct requestData;


//? 请求数据的结构
struct requestData
{
private:
    int againTimes;                         //? 请求重复的次数
    std::string path;
    int fd;                                 //? 当前请求数据来自的文件描述符
    int epollfd;                            //? 当前文件描述符属于的epollfd
    // content的内容用完就清
    std::string content;
    int method;
    int HTTPversion;
    std::string file_name;
    int now_read_pos;                       //? 当前读取位置？
    int state;                              //? 当前请求的状态
    int h_state;                            //? 请求头部的状态
    bool isfinish;
    bool keep_alive;                        //? 链接是否保活
    std::unordered_map<std::string, std::string> headers;
    mytimer *timer;                         //? 每个请求连接都维护一个定时器

private:
    int parse_URI();
    int parse_Headers();
    int analysisRequest();

public:

    requestData();
    requestData(int _epollfd, int _fd, std::string _path);
    ~requestData();
    void addTimer(mytimer *mtimer);
    void reset();
    void seperateTimer();
    int getFd();
    void setFd(int _fd);
    void handleRequest();
    void handleError(int fd, int err_num, std::string short_msg);
};

//? 定时器，定时剔除超时请求
struct mytimer
{
    bool deleted;
    size_t expired_time;
    requestData *request_data; //? 每一个timer指向一个请求数据，当超时时就剔除？

    mytimer(requestData *_request_data, int timeout);
    ~mytimer();
    void update(int timeout);
    bool isvalid();
    void clearReq();
    void setDeleted();
    bool isDeleted() const;
    size_t getExpTime() const;
};

struct timerCmp
{
    bool operator()(const mytimer *a, const mytimer *b) const;
};
#endif