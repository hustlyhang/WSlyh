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

private:

    std::string m_strMethod;//���󷽷�

    std::string m_strUrl;//����·��[�������������]

    std::map<std::string, std::string> m_mRequestParams;//�������

    std::string m_strProtocol;//Э��
    std::string m_strVersion;//�汾

    std::map<std::string, std::string> m_strHeaders;//���е�����ͷ

    std::string m_strBody;//������

    int m_iNextPos = 0;//��һ��λ�õ�

    HttpRequestDecodeState m_eDecodeState = HttpRequestDecodeState::START;//����״̬
};



class CHttp {
public:
    CHttp() {}
    ~CHttp() {};

	bool Read();			// loop�ж�ȡһ�Σ�����le��et���в�ͬ��ʵ��
	bool Write();			// loop�е��յ�EPOLLOUTʱ���ô˺���������������д��sock
	bool ParseRead();		// �����յ������ݣ���������Ӧ������
	bool ParseWrite();		// ���ݽ����������ݣ�������������
	void HttpParse();			// �̳߳�����Ҫ���õĺ��������Ϸ�����ǰ���յ�������
    void Init(int _sockfd, const sockaddr_in &_addr, int _triggerMode);
#ifdef DEBUG_T
    void Test(const char* _str, int _len) {
        m_sHttpParse.ParseInternal(_str, _len);
    }
    void Show() {
        std::cout << "Protocol" << m_sHttpParse.GetProtocol() << std::endl;
        std::cout << "Method"<<m_sHttpParse.GetMethod() << std::endl;
        std::cout << "tUrl"<<m_sHttpParse.GetUrl() << std::endl;
        std::cout << "Version"<<m_sHttpParse.GetVersion() << std::endl;
        std::cout << "Body" << m_sHttpParse.GetBody() << std::endl;
        std::cout << "Header:" << std::endl;
        for (auto x : m_sHttpParse.GetHeaders()) {
            std::cout << x.first << std::endl;
            std::cout << x.second << std::endl;
        }
        std::cout << "Header:" << std::endl;
        for (auto x : m_sHttpParse.GetRequestParams()) {
            std::cout << x.first << std::endl;
            std::cout << x.second << std::endl;
        }
    }
#endif // DEBUG


public:
	
	static int m_iEpollFd;
	static int m_iHttpCnt;		// ����������
private:
	int m_iSockFd;		// http�󶨵�sock
	sockaddr_in m_sAddr;// ��¼���ӵĵ�ַ�˿�
	char m_aReadData[MAX_READ_DATA_BUFF_SIZE];	    // ��������
    char m_aWriteData[MAX_WRITE_DATA_BUFF_SIZE];    // д����
    int m_iDateLen;
    int m_iTriggerMode;         // ����ģʽ 0 LT 1 ET
    int m_iReadIdx;             // ���������е�ǰ��ȡ����λ��
    SHttpRequest m_sHttpParse;

    struct stat m_sFileStat;    // �ļ�
    struct iovec m_sIv[2];
    int m_iIvCount;
};
#endif