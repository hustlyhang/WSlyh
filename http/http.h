#ifndef HTTP_H
#define HTTP_H
#include "../log/log.h"
#include <map>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>	// stat
#include <netinet/in.h>	// sockaddr_in
#include <sys/stat.h>   // stat

#define CR '\r'
#define LF '\n'
#define MAX_READ_DATA_BUFF_SIZE 5 * 1024 // 5k
#define MAX_WRITE_DATA_BUFF_SIZE 5 * 1024 // 5k
#define MAX_FILENAME_LEN 1024

// #define DEBUG_T

extern int pipeFd[2];

typedef struct {
	char* m_pBegin = nullptr;	//字符串开始位置
	char* m_pEnd = nullptr;		//字符串结束位置
	operator std::string() const {
		return std::string(m_pBegin, m_pEnd);
	}
}StringBuffer;

enum class HttpRequestDecodeState {
    INVALID,//无效
    INVALID_METHOD,//无效请求方法
    INVALID_URI,//无效的请求路径
    INVALID_VERSION,//无效的协议版本号
    INVALID_HEADER,//无效请求头


    START,//请求行开始
    METHOD,//请求方法

    BEFORE_URI,//请求连接前的状态，需要'/'开头
    IN_URI,//url处理
    BEFORE_URI_PARAM_KEY,//URL请求参数键之前
    URI_PARAM_KEY,//URL请求参数键
    BEFORE_URI_PARAM_VALUE,//URL的参数值之前
    URI_PARAM_VALUE,//URL请求参数值

    BEFORE_PROTOCOL,//协议解析之前
    PROTOCOL,//协议

    BEFORE_VERSION,//版本开始前
    VERSION_SPLIT,//版本分隔符 '.'
    VERSION,//版本

    HEADER_KEY,

    HEADER_BEFORE_COLON,//冒号之前
    HEADER_AFTER_COLON,//冒号
    HEADER_VALUE,//值

    WHEN_CR,//遇到一个回车之后

    CR_LF,//回车换行

    CR_LF_CR,//回车换行之后的状态


    BODY,//请求体

    COMPLETE,//完成

    OPEN,// 数据未接收完全
};


struct SHttpRequest {
    const std::string& GetMethod() const;
    const std::string& GetUrl() const;
    const std::map<std::string, std::string>& GetRequestParams() const;
    const std::string& GetProtocol() const;
    const std::string& GetVersion() const;
    const std::map<std::string, std::string>& GetHeaders() const;
    const std::string& GetBody() const;
    const HttpRequestDecodeState GetStatus() const;
    void ParseInternal(const char* buf, int size);
    int GetLen();
    void Init();
private:
    std::string m_strMethod;//请求方法
    std::string m_strUrl;//请求路径[不包含请求参数]
    std::map<std::string, std::string> m_mRequestParams;//请求参数
    std::string m_strProtocol;//协议
    std::string m_strVersion;//版本
    std::map<std::string, std::string> m_mHeaders;//所有的请求头
    std::string m_strBody;//请求体
    int m_iNextPos = 0;//下一个位置的
    bool m_bIsOpen = false; // 当前解析http请求是否未接收完全
    HttpRequestDecodeState m_eDecodeState = HttpRequestDecodeState::START;//解析状态
};



class CHttp {
public:
    CHttp() {}
    ~CHttp() {};

	bool Read();			    // loop中读取一次，根据le和et会有不同的实现
	bool Write();			    // loop中当收到EPOLLOUT时调用此函数将处理后的内容写入sock
    HttpRequestDecodeState ParseRead();		            // 对于收到的数据，解析出相应的数据
	bool ParseWrite(HttpRequestDecodeState);		    // 根据解析出的数据，构建回送请求
	void HttpParse();			                        // 线程池中需要调用的函数，不断分析当前所收到的数据
    void Init(int _sockfd, const sockaddr_in &_addr, int _triggerMode);
    void CloseHttp();           // 关闭连接
    bool AddLine(const char* _format, ...);             // 向读缓冲区中添加一行内容
    bool AddStatusLine(int _status, const char* _title);// 添加状态行
    bool AddHeaders(int _contentlen);                   // 添加header
    bool AddContentLength(int _contentlen);             // 添加Content-Length
    bool AddContentType();                              // 添加内容形式
    bool AddLinger();                                   // 添加连接方式
    bool AddBlankLine();                                // 添加空行
    bool AddContent(const char* _content);              // 添加内容
    void unmmap();                                      // 因为使用了文件映射，当传送完后需要解除映射
    sockaddr_in *GetAddress() {
        return &m_sAddr;
    }
    void SendErrSig() {
        int ms = 100;
        send(pipeFd[1], (char *)&ms, 1, 0);
    }
#ifdef DEBUG_T
    void Test(const char* _str, int _len) {
        m_sHttpParse.ParseInternal(_str, _len);
    }
    void Show() {
        LOG_INFO("Protocol:%s", m_sHttpParse.GetProtocol().c_str());
        LOG_INFO("Method:%s", m_sHttpParse.GetMethod().c_str());
        LOG_INFO("Url:%s", m_sHttpParse.GetUrl().c_str());
        LOG_INFO("Version:%s", m_sHttpParse.GetVersion().c_str());
        LOG_INFO("Body:%s", m_sHttpParse.GetBody().c_str());

        for (auto x : m_sHttpParse.GetHeaders()) 
            LOG_INFO("%s:%s", x.first.c_str(), x.second.c_str());

        for (auto x : m_sHttpParse.GetRequestParams())
            LOG_INFO("%s:%s", x.first.c_str(), x.second.c_str());
        LOG_INFO("状态机目前状态为：%d", m_sHttpParse.GetStatus());
    }
#endif // DEBUG
public:
	static int m_iEpollFd;
	static int m_iHttpCnt;		                // 所有连接数
    static char m_aFilePathPrefix[100];         // 请求问价路径前缀
    // static int m_iPipFd;                        // 出错时向主进程发送信号的fd
    int m_iState;                               // 记录当前连接的状态，1为读，2为写
    

private:
	int m_iSockFd;		                        // http绑定的sock
	sockaddr_in m_sAddr;                            // 记录连接的地址端口
	char m_aReadData[MAX_READ_DATA_BUFF_SIZE];	    // 读的数据
    char m_aWriteData[MAX_WRITE_DATA_BUFF_SIZE];    // 写数据
    char *m_pFileAddr;          // 文件映射时的起始地址
    int m_iDataLen;             // 所有待发送数据的长度
    int m_iDataSendLen;         // 已发送数据的长度
    int m_iTriggerMode;         // 触发模式 0 LT 1 ET
    int m_iReadIdx;             // 读缓冲区中当前读取内容位置
    int m_iWriteIdx;            // 写缓冲区中当前位置
    SHttpRequest m_sHttpParse;
    struct stat m_sFileStat;    // 文件
    struct iovec m_sIv[2];
    int m_iIvCount;
    bool m_bLinger;             // 连接方式
    char m_aFile[MAX_FILENAME_LEN] = {"./1.html"};
};
#endif