#ifndef REQUESTDATA
#define REQUESTDATA
#include <string.h>
#include <unordered_map>

/*
    requestData����Ҫ������ÿ�����Ӷ���������У�����ͷ�������壬֧��post��get��
    ͬʱÿ��������Ҫ��һ��timer����¼��ʱʱ��
*/


//? �������������е�״̬��������һ����
const int STATE_PARSE_URI = 1;
const int STATE_PARSE_HEADERS = 2;
const int STATE_RECV_BODY = 3;
const int STATE_ANALYSIS = 4;
const int STATE_FINISH = 5;

const int MAX_BUFF = 4096;

// ��������ֵ��Ƕ���������,������Request Aborted,
// �����������������û�дﵽ��ԭ��,
// �������������Գ���һ���Ĵ���������
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



// ��������ͷ��״̬
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
    int nTryTimes;                        // ÿ�������ԵĴ���
    int nFd;                              // ��¼ÿ��������ļ�������
    int nEpoll_fd;                        // ��¼��ǰfd������epoll_fd
    std::string strPath;                  // �ļ���·��
    std::string strContent;               // ��ʱ�洢ÿ�����������
    std::string strFilename;              // �����ļ���
    int nMethod;                          // ��¼��ǰ����ķ�����post��get
    int nHTTPversion;                     // ��¼��ǰ�����Э��汾��
    CTimer* timer;                        // ÿ�������һ����ʱ��
    // ������ ��������ʱ��Ҫ�ĸ�������
    int nState;                            // �������������״̬
    int nState_header;                     // ��������ͷ��״̬
    int nParse_pos;                        // ����λ��
    bool bIsOver;                          // 
    bool bKeep_alive;                      // �����Ƿ񱣻�
    std::unordered_map<std::string, std::string> mHeaders;


  public:
    CRequestData();
    CRequestData(int _fd);
    ~CRequestData();
    void HandleRequest();                 // ��������
    void HandleError(int _fd, int error, std::string short_msg);
    void SetFd(int _fd);                  // ���ļ��������͵�ǰ���
    int GetFd();                          // ��õ�ǰ�󶨵�fd
    void Reset();                         // ��������࣬���ڸ��ã������ظ�new
    void SeperateTimer();                 // ���붨ʱ��
    void AddTimer(CTimer* cTimer);        // �󶨶�ʱ��
    

  private:
    int ParseURI();
    int ParseHeader();
    int ParseAnalysis();
};


class CTimer{
  private:
    bool bIsDelete;                       // �Ƿ�ɾ��
    size_t lExpiredTime;                  // ����ʱ��
    CRequestData* requestdata;            // ��ʱ���󶨵�����
  
  public:
    CTimer(CRequestData* _requestdata, size_t _timeout);
    ~CTimer();
    void Update(size_t _timeout);         // ���¼�ʱ���Ĺ���ʱ��
    bool IsValid();                       // ����ʱ���Ƿ���Ч
    void SetDelete();                     // ����ʱ��ɾ��
    bool IsDelete();
    void ClearRe();                       // ���ͼ�ʱ����ص�re���
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