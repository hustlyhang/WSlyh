#ifndef REQUESTDATA
#define REQUESTDATA
#include <string.h>
#include <unordered_map>

/*
    requestData类主要负责处理每个连接额，分析请求行，请求头，请求体，支持post和get，
    同时每个请求需要绑定一个timer，记录过时时间
*/


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



// 处理请求头的状态
enum class HEADERS_STATE {
    h_start = 0,
    h_key,
    h_colon,
    h_space_after_colon,
    h_value,
    h_CR,
    h_LF,
    h_end_CR,
    h_end_LF
};
class MyType;
class CTimer;
class CRequestData;
class CRequestData{
  private:
    int nTryTimes;                        // 每个请求尝试的次数
    int nFd;                              // 记录每个请求的文件描述符
    int nEpoll_fd;                        // 记录当前fd所属的epoll_fd
    std::string strPath;                  // 文件的路径
    std::string strContent;               // 临时存储每个请求的内容
    std::string strFilename;              // 请求文件名
    int nMethod;                          // 记录当前请求的方法，post和get
    int nHTTPversion;                     // 记录当前请求的协议版本号
    CTimer* timer;                        // 每个请求绑定一个计时器
    // 下面是 分析请求时需要的辅助变量
    int nState;                            // 分析整个请求的状态
    int nState_header;                     // 分析请求头的状态
    int nParse_pos;                        // 处理位置
    bool bIsOver;                          // 
    bool bKeep_alive;                      // 连接是否保活
    std::unordered_map<std::string, std::string> mHeaders;


  public:
    CRequestData();
    CRequestData(int _fd);
    ~CRequestData();
    void HandleRequest();                 // 处理请求
    void HandleError(int _fd, int error, std::string short_msg);
    void SetFd(int _fd);                  // 将文件描述符和当前类绑定
    int GetFd();                          // 获得当前绑定的fd
    void Reset();                         // 重置这个类，便于复用，避免重复new
    void SeperateTimer();                 // 分离定时器
    void AddTimer(CTimer* cTimer);        // 绑定定时器
    

  private:
    int ParseURI();
    int ParseHeader();
    int ParseAnalysis();
};


class CTimer{
  private:
    bool bIsDelete;                       // 是否被删除
    size_t lExpiredTime;                  // 过期时间
    CRequestData* requestdata;            // 计时器绑定的请求
  
  public:
    CTimer(CRequestData* _requestdata, size_t _timeout);
    ~CTimer();
    void Update(size_t _timeout);         // 更新计时器的过期时间
    bool IsValid();                       // 检查计时器是否还有效
    void SetDelete();                     // 将计时器删除
    bool IsDelete();
    void ClearRe();                       // 将和计时器相关的re清空
    size_t GetExpiredTime();
};

class MyType{
  private:
    static pthread_mutex_t lock;
    static std::unordered_map<std::string, std::string> mytype;
    MyType();
    MyType(const MyType&);
  
  public:
    static std::string GetType(const std::string &str);
};
struct TimerCmp{
    bool operator()(const CTimer *a, const CTimer *b) const;
};

#endif // !REQUESTDATA