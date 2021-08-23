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

#define DEBUG_T

typedef struct {
	char* m_pBegin = nullptr;	//�ַ�����ʼλ��
	char* m_pEnd = nullptr;		//�ַ�������λ��
	operator std::string() const {
		return std::string(m_pBegin, m_pEnd);
	}
}StringBuffer;

enum class HttpRequestDecodeState {
    INVALID,//��Ч
    INVALID_METHOD,//��Ч���󷽷�
    INVALID_URI,//��Ч������·��
    INVALID_VERSION,//��Ч��Э��汾��
    INVALID_HEADER,//��Ч����ͷ


    START,//�����п�ʼ
    METHOD,//���󷽷�

    BEFORE_URI,//��������ǰ��״̬����Ҫ'/'��ͷ
    IN_URI,//url����
    BEFORE_URI_PARAM_KEY,//URL���������֮ǰ
    URI_PARAM_KEY,//URL���������
    BEFORE_URI_PARAM_VALUE,//URL�Ĳ���ֵ֮ǰ
    URI_PARAM_VALUE,//URL�������ֵ

    BEFORE_PROTOCOL,//Э�����֮ǰ
    PROTOCOL,//Э��

    BEFORE_VERSION,//�汾��ʼǰ
    VERSION_SPLIT,//�汾�ָ��� '.'
    VERSION,//�汾

    HEADER_KEY,

    HEADER_BEFORE_COLON,//ð��֮ǰ
    HEADER_AFTER_COLON,//ð��
    HEADER_VALUE,//ֵ

    WHEN_CR,//����һ���س�֮��

    CR_LF,//�س�����

    CR_LF_CR,//�س�����֮���״̬


    BODY,//������

    COMPLETE,//���

    OPEN,// ����δ������ȫ
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
    std::string m_strMethod;//���󷽷�
    std::string m_strUrl;//����·��[�������������]
    std::map<std::string, std::string> m_mRequestParams;//�������
    std::string m_strProtocol;//Э��
    std::string m_strVersion;//�汾
    std::map<std::string, std::string> m_mHeaders;//���е�����ͷ
    std::string m_strBody;//������
    int m_iNextPos = 0;//��һ��λ�õ�
    bool m_bIsOpen = false; // ��ǰ����http�����Ƿ�δ������ȫ
    HttpRequestDecodeState m_eDecodeState = HttpRequestDecodeState::START;//����״̬
};



class CHttp {
public:
    CHttp() {}
    ~CHttp() {};

	bool Read();			    // loop�ж�ȡһ�Σ�����le��et���в�ͬ��ʵ��
	bool Write();			    // loop�е��յ�EPOLLOUTʱ���ô˺���������������д��sock
    HttpRequestDecodeState ParseRead();		            // �����յ������ݣ���������Ӧ������
	bool ParseWrite(HttpRequestDecodeState);		    // ���ݽ����������ݣ�������������
	void HttpParse();			                        // �̳߳�����Ҫ���õĺ��������Ϸ�����ǰ���յ�������
    void Init(int _sockfd, const sockaddr_in &_addr, int _triggerMode);
    void CloseHttp();           // �ر�����
    bool AddLine(const char* _format, ...);             // ��������������һ������
    bool AddStatusLine(int _status, const char* _title);// ���״̬��
    bool AddHeaders(int _contentlen);                   // ���header
    bool AddContentLength(int _contentlen);             // ���Content-Length
    bool AddContentType();                              // ���������ʽ
    bool AddLinger();                                   // ������ӷ�ʽ
    bool AddBlankLine();                                // ��ӿ���
    bool AddContent(const char* _content);              // �������
    void unmmap();                                      // ��Ϊʹ�����ļ�ӳ�䣬�����������Ҫ���ӳ��
    sockaddr_in *GetAddress() {
        return &m_sAddr;
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
        LOG_INFO("״̬��Ŀǰ״̬Ϊ��%d", m_sHttpParse.GetStatus());
    }
#endif // DEBUG
public:
	static int m_iEpollFd;
	static int m_iHttpCnt;		// ����������
    static char m_aFilePathPrefix[100];   // �����ʼ�·��ǰ׺
private:
	int m_iSockFd;		// http�󶨵�sock
	sockaddr_in m_sAddr;// ��¼���ӵĵ�ַ�˿�
	char m_aReadData[MAX_READ_DATA_BUFF_SIZE];	    // ��������
    char m_aWriteData[MAX_WRITE_DATA_BUFF_SIZE];    // д����
    char *m_pFileAddr;          // �ļ�ӳ��ʱ����ʼ��ַ
    int m_iDataLen;             // ���д��������ݵĳ���
    int m_iDataSendLen;         // �ѷ������ݵĳ���
    int m_iTriggerMode;         // ����ģʽ 0 LT 1 ET
    int m_iReadIdx;             // ���������е�ǰ��ȡ����λ��
    int m_iWriteIdx;            // д�������е�ǰλ��
    SHttpRequest m_sHttpParse;
    struct stat m_sFileStat;    // �ļ�
    struct iovec m_sIv[2];
    int m_iIvCount;
    bool m_bLinger;             // ���ӷ�ʽ
    char m_aFile[MAX_FILENAME_LEN] = {"./1.html"};
};
#endif