#include "http.h"
#include <functional>


#include "http.h"
#include <string.h>
#include <vector>

//Э�����
void SHttpRequest::TryDecode(const std::string& buf) {
    this->ParseInternal(buf.c_str(), buf.size());
}

//����������
void SHttpRequest::ParseInternal(const char* buf, int size) {

    StringBuffer method;
    StringBuffer url;

    StringBuffer requestParamKey;
    StringBuffer requestParamValue;

    StringBuffer protocol;
    StringBuffer version;

    StringBuffer headerKey;
    StringBuffer headerValue;

    int bodyLength = 0;

    char* p0 = const_cast<char*>(buf + m_iNextPos);//ȥ��const����

    while (m_eDecodeState != HttpRequestDecodeState::INVALID
        && m_eDecodeState != HttpRequestDecodeState::INVALID_METHOD
        && m_eDecodeState != HttpRequestDecodeState::INVALID_URI
        && m_eDecodeState != HttpRequestDecodeState::INVALID_VERSION
        && m_eDecodeState != HttpRequestDecodeState::INVALID_HEADER
        && m_eDecodeState != HttpRequestDecodeState::COMPLETE
        && m_iNextPos < size) {

        char ch = *p0;//��ǰ���ַ�
        char* p = p0++;//ָ��ƫ��
        int scanPos = m_iNextPos++;//��һ��ָ������ƫ��

        switch (m_eDecodeState) {
        case HttpRequestDecodeState::START: {
            //�ո� ���� �س�������
            if (ch == CR || ch == LF || isblank(ch)) {
                //do nothing
            }
            else if (isupper(ch)) {//�ж��ǲ��Ǵ�д�ַ������ǵĻ�����Ч��
                method.m_pBegin = p;//��������ʼ��
                m_eDecodeState = HttpRequestDecodeState::METHOD;//���������һ���ַ�����ʼ��������
            }
            else {
                m_eDecodeState = HttpRequestDecodeState::INVALID;
            }
            break;
        }
        case HttpRequestDecodeState::METHOD: {
            //������Ҫ��д��ĸ����д��ĸ�ͼ���
            if (isupper(ch)) {
                //do nothing
            }
            else if (isblank(ch)) {//�ո�˵������������������һ����ʼ����URI
                method.m_pEnd = p;//������������
                m_strMethod = method;
                m_eDecodeState = HttpRequestDecodeState::BEFORE_URI;
            }
            else {
                m_eDecodeState = HttpRequestDecodeState::INVALID_METHOD;//�����������Ч�����󷽷�
            }
            break;
        }
        case HttpRequestDecodeState::BEFORE_URI: {
            //��������ǰ�Ĵ�����Ҫ'/'��ͷ
            if (ch == '/') {
                url.m_pBegin = p;
                m_eDecodeState = HttpRequestDecodeState::IN_URI;//��һ�����ǿ�ʼ��������
            }
            else if (isblank(ch)) {
                //do nothing
            }
            else {
                m_eDecodeState = HttpRequestDecodeState::INVALID;//��Ч��
            }
            break;
        }
        case HttpRequestDecodeState::IN_URI: {
            //��ʼ��������·�����ַ���
            if (ch == '?') {//תΪ���������keyֵ
                url.m_pEnd = p;
                m_strUrl = url;
                m_eDecodeState = HttpRequestDecodeState::BEFORE_URI_PARAM_KEY;
            }
            else if (isblank(ch)) {//�����ո�����·��������ɣ���ʼ����Э��
                url.m_pEnd = p;
                m_strUrl = url;
                m_eDecodeState = HttpRequestDecodeState::BEFORE_PROTOCOL;
            }
            else {
                //do nothing
            }
            break;
        }
        case HttpRequestDecodeState::BEFORE_URI_PARAM_KEY: {
            if (isblank(ch) || ch == LF || ch == CR) {
                m_eDecodeState = HttpRequestDecodeState::INVALID_URI;
            }
            else {
                requestParamKey.m_pBegin = p;
                m_eDecodeState = HttpRequestDecodeState::URI_PARAM_KEY;
            }
            break;
        }
        case HttpRequestDecodeState::URI_PARAM_KEY: {
            if (ch == '=') {
                requestParamKey.m_pEnd = p;
                m_eDecodeState = HttpRequestDecodeState::BEFORE_URI_PARAM_VALUE;//��ʼ��������ֵ
            }
            else if (isblank(ch)) {
                m_eDecodeState = HttpRequestDecodeState::INVALID_URI;//��Ч���������
            }
            else {
                //do nothing
            }
            break;
        }
        case HttpRequestDecodeState::BEFORE_URI_PARAM_VALUE: {
            if (isblank(ch) || ch == LF || ch == CR) {
                m_eDecodeState = HttpRequestDecodeState::INVALID_URI;
            }
            else {
                requestParamValue.m_pBegin = p;
                m_eDecodeState = HttpRequestDecodeState::URI_PARAM_VALUE;
            }
            break;
        }
        case HttpRequestDecodeState::URI_PARAM_VALUE: {
            if (ch == '&') {
                requestParamValue.m_pEnd = p;
                //�ջ�һ���������
                m_mRequestParams.insert({ requestParamKey, requestParamValue });
                m_eDecodeState = HttpRequestDecodeState::BEFORE_URI_PARAM_KEY;
            }
            else if (isblank(ch)) {
                requestParamValue.m_pEnd = p;
                //�ո�Ҳ�ջ�һ���������
                m_mRequestParams.insert({ requestParamKey, requestParamValue });
                m_eDecodeState = HttpRequestDecodeState::BEFORE_PROTOCOL;
            }
            else {
                //do nothing
            }
            break;
        }
        case HttpRequestDecodeState::BEFORE_PROTOCOL: {
            if (isblank(ch)) {
                //do nothing
            }
            else {
                protocol.m_pBegin = p;
                m_eDecodeState = HttpRequestDecodeState::PROTOCOL;
            }
            break;
        }
        case HttpRequestDecodeState::PROTOCOL: {
            //��������Э��
            if (ch == '/') {
                protocol.m_pEnd = p;
                m_strProtocol = protocol;
                m_eDecodeState = HttpRequestDecodeState::BEFORE_VERSION;
            }
            else {
                //do nothing
            }
            break;
        }
        case HttpRequestDecodeState::BEFORE_VERSION: {
            if (isdigit(ch)) {
                version.m_pBegin = p;
                m_eDecodeState = HttpRequestDecodeState::VERSION;
            }
            else {
                m_eDecodeState = HttpRequestDecodeState::INVALID_VERSION;
            }
            break;
        }
        case HttpRequestDecodeState::VERSION: {
            //Э�����������������ֻ���. �Ͳ���
            if (ch == CR) {
                version.m_pEnd = p;
                m_strVersion = version;
                m_eDecodeState = HttpRequestDecodeState::WHEN_CR;
            }
            else if (ch == '.') {
                //�����汾�ָ�
                m_eDecodeState = HttpRequestDecodeState::VERSION_SPLIT;
            }
            else if (isdigit(ch)) {
                //do nothing
            }
            else {
                m_eDecodeState = HttpRequestDecodeState::INVALID_VERSION;//���ܲ�������
            }
            break;
        }
        case HttpRequestDecodeState::VERSION_SPLIT: {
            //�����汾�ָ�����ַ����������֣�����������Ǵ���
            if (isdigit(ch)) {
                m_eDecodeState = HttpRequestDecodeState::VERSION;
            }
            else {
                m_eDecodeState = HttpRequestDecodeState::INVALID_VERSION;
            }
            break;
        }
        case HttpRequestDecodeState::HEADER_KEY: {
            //ð��ǰ������пո�
            if (isblank(ch)) {
                headerKey.m_pEnd = p;
                m_eDecodeState = HttpRequestDecodeState::HEADER_BEFORE_COLON;//ð��֮ǰ��״̬
            }
            else if (ch == ':') {
                headerKey.m_pEnd = p;
                m_eDecodeState = HttpRequestDecodeState::HEADER_AFTER_COLON;//ð��֮���״̬
            }
            else {
                //do nothing
            }
            break;
        }
        case HttpRequestDecodeState::HEADER_BEFORE_COLON: {
            if (isblank(ch)) {
                //do nothing
            }
            else if (ch == ':') {
                m_eDecodeState = HttpRequestDecodeState::HEADER_AFTER_COLON;
            }
            else {
                //ð��֮ǰ��״̬�����ǿո�֮��������ַ�
                m_eDecodeState = HttpRequestDecodeState::INVALID_HEADER;
            }
            break;
        }
        case HttpRequestDecodeState::HEADER_AFTER_COLON: {
            if (isblank(ch)) {//ֵ֮ǰ�����ո���������
                //do nothing
            }
            else {
                headerValue.m_pBegin = p;
                m_eDecodeState = HttpRequestDecodeState::HEADER_VALUE;//��ʼ����ֵ
            }
            break;
        }
        case HttpRequestDecodeState::HEADER_VALUE: {
            if (ch == CR) {
                headerValue.m_pEnd = p;
                m_strHeaders.insert({ headerKey, headerValue });
                m_eDecodeState = HttpRequestDecodeState::WHEN_CR;
            }
            break;
        }
        case HttpRequestDecodeState::WHEN_CR: {
            if (ch == LF) {
                //����ǻس����ɻ�����һ��
                m_eDecodeState = HttpRequestDecodeState::CR_LF;
            }
            else {
                m_eDecodeState = HttpRequestDecodeState::INVALID;
            }
            break;
        }
        case HttpRequestDecodeState::CR_LF: {
            if (ch == CR) {
                //�����CR_LF״̬֮����CR����ô�����е������ζ����
                m_eDecodeState = HttpRequestDecodeState::CR_LF_CR;
            }
            else if (isblank(ch)) {
                m_eDecodeState = HttpRequestDecodeState::INVALID;
            }
            else {
                //������ǣ���ô�Ϳ�������һ������ͷ�ˣ��ǾͿ�ʼ��������ͷ
                headerKey.m_pBegin = p;
                m_eDecodeState = HttpRequestDecodeState::HEADER_KEY;
            }
            break;
        }
        case HttpRequestDecodeState::CR_LF_CR: {
            if (ch == LF) {
                //�����\r����\n ��ô�ж��ǲ�����Ҫ����������
                if (m_strHeaders.count("Content-Length") > 0) {
                    bodyLength = atoi(m_strHeaders["Content-Length"].c_str());
                    if (bodyLength > 0) {
                        m_eDecodeState = HttpRequestDecodeState::BODY;//����������
                    }
                    else {
                        m_eDecodeState = HttpRequestDecodeState::COMPLETE;//�����
                    }
                }
                else {
                    if (scanPos < size) {
                        bodyLength = size - scanPos;
                        m_eDecodeState = HttpRequestDecodeState::BODY;//����������
                    }
                    else {
                        m_eDecodeState = HttpRequestDecodeState::COMPLETE;
                    }
                }
            }
            else {
                m_eDecodeState = HttpRequestDecodeState::INVALID_HEADER;
            }
            break;
        }
        case HttpRequestDecodeState::BODY: {
            //����������
            m_strBody.assign(p, bodyLength);
            m_eDecodeState = HttpRequestDecodeState::COMPLETE;
            break;
        }
        default:
            break;
        }
    }
}

const std::string& SHttpRequest::GetMethod() const {
    return m_strMethod;
}

const std::string& SHttpRequest::GetUrl() const {
    return m_strUrl;
}

const std::map<std::string, std::string>& SHttpRequest::GetRequestParams() const {
    return m_mRequestParams;
}

const std::string& SHttpRequest::GetProtocol() const {
    return m_strProtocol;
}

const std::string& SHttpRequest::GetVersion() const {
    return m_strVersion;
}

const std::map<std::string, std::string>& SHttpRequest::GetHeaders() const {
    return m_strHeaders;
}

const std::string& SHttpRequest::GetBody() const {
    return m_strBody;
}

const HttpRequestDecodeState SHttpRequest::GetStatus() const {
    return m_eDecodeState;
}

// �ļ�����������




// ��̬������ʼ��
int CHttp::m_iEpollFd = -1;
int CHttp::m_iHttpCnt = 0;

void CHttp::Init(int _sockfd, const sockaddr_in& _addr) {
    m_iSockFd = _sockfd;
    m_sAddr = _addr;

}