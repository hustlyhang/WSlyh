#ifndef HTTP_H
#define HTTP_H
#include "../log/log.h"
#include <map>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>	// 
#include <netinet/in.h>	// sockaddr_in

#define CR '\r'
#define LF '\n'
#define MAX_DATA_BUFF_SIZE 5 * 1024 // 5k

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
};


struct SHttpRequest {

    /**
     * 解析http协议
     * @param buf
     * @return
     */
    void TryDecode(const std::string& buf);

    const std::string& GetMethod() const;

    const std::string& GetUrl() const;

    const std::map<std::string, std::string>& GetRequestParams() const;

    const std::string& GetProtocol() const;

    const std::string& GetVersion() const;

    const std::map<std::string, std::string>& GetHeaders() const;

    const std::string& GetBody() const;

    const HttpRequestDecodeState GetStatus() const;

private:

    void ParseInternal(const char* buf, int size);

private:

    std::string m_strMethod;//请求方法

    std::string m_strUrl;//请求路径[不包含请求参数]

    std::map<std::string, std::string> m_mRequestParams;//请求参数

    std::string m_strProtocol;//协议
    std::string m_strVersion;//版本

    std::map<std::string, std::string> m_strHeaders;//所有的请求头

    std::string m_strBody;//请求体

    int m_iNextPos = 0;//下一个位置的

    HttpRequestDecodeState m_eDecodeState = HttpRequestDecodeState::START;//解析状态
};



class CHttp {
public:
    CHttp() {}
    ~CHttp(){}

	void Read();			// loop中读取一次，根据le和et会有不同的实现
	void Write();			// loop中当收到EPOLLOUT时调用此函数将处理后的内容写入sock
	void ParseRead();		// 对于收到的数据，解析出相应的数据
	void ParseWrite();		// 根据解析出的数据，构建回送请求
	void HttpParse();			// 线程池中需要调用的函数，不断分析当前所收到的数据
    void Init(int _sockfd, const sockaddr_in &_addr);
public:
	
	static int m_iEpollFd;
	static int m_iHttpCnt;		// 所有连接数
private:
	int m_iSockFd;		// http绑定的sock
	sockaddr_in m_sAddr;// 记录连接的地址端口
	char m_cData[MAX_DATA_BUFF_SIZE];	// 数据
    int m_iDateLen;
};
#endif