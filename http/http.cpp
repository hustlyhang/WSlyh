#include "http.h"
#include <functional>


#include "http.h"
#include <string.h>
#include <vector>

//协议解析
void SHttpRequest::TryDecode(const std::string& buf) {
    this->ParseInternal(buf.c_str(), buf.size());
}

//解析请求行
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

    char* p0 = const_cast<char*>(buf + m_iNextPos);//去掉const限制

    while (m_eDecodeState != HttpRequestDecodeState::INVALID
        && m_eDecodeState != HttpRequestDecodeState::INVALID_METHOD
        && m_eDecodeState != HttpRequestDecodeState::INVALID_URI
        && m_eDecodeState != HttpRequestDecodeState::INVALID_VERSION
        && m_eDecodeState != HttpRequestDecodeState::INVALID_HEADER
        && m_eDecodeState != HttpRequestDecodeState::COMPLETE
        && m_iNextPos < size) {

        char ch = *p0;//当前的字符
        char* p = p0++;//指针偏移
        int scanPos = m_iNextPos++;//下一个指针往后偏移

        switch (m_eDecodeState) {
        case HttpRequestDecodeState::START: {
            //空格 换行 回车都继续
            if (ch == CR || ch == LF || isblank(ch)) {
                //do nothing
            }
            else if (isupper(ch)) {//判断是不是大写字符，不是的话是无效的
                method.m_pBegin = p;//方法的起始点
                m_eDecodeState = HttpRequestDecodeState::METHOD;//如果遇到第一个字符，开始解析方法
            }
            else {
                m_eDecodeState = HttpRequestDecodeState::INVALID;
            }
            break;
        }
        case HttpRequestDecodeState::METHOD: {
            //方法需要大写字母，大写字母就继续
            if (isupper(ch)) {
                //do nothing
            }
            else if (isblank(ch)) {//空格，说明方法解析结束，下一步开始解析URI
                method.m_pEnd = p;//方法解析结束
                m_strMethod = method;
                m_eDecodeState = HttpRequestDecodeState::BEFORE_URI;
            }
            else {
                m_eDecodeState = HttpRequestDecodeState::INVALID_METHOD;//其他情况是无效的请求方法
            }
            break;
        }
        case HttpRequestDecodeState::BEFORE_URI: {
            //请求连接前的处理，需要'/'开头
            if (ch == '/') {
                url.m_pBegin = p;
                m_eDecodeState = HttpRequestDecodeState::IN_URI;//下一步就是开始处理连接
            }
            else if (isblank(ch)) {
                //do nothing
            }
            else {
                m_eDecodeState = HttpRequestDecodeState::INVALID;//无效的
            }
            break;
        }
        case HttpRequestDecodeState::IN_URI: {
            //开始处理请求路径的字符串
            if (ch == '?') {//转为处理请求的key值
                url.m_pEnd = p;
                m_strUrl = url;
                m_eDecodeState = HttpRequestDecodeState::BEFORE_URI_PARAM_KEY;
            }
            else if (isblank(ch)) {//遇到空格，请求路径解析完成，开始解析协议
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
                m_eDecodeState = HttpRequestDecodeState::BEFORE_URI_PARAM_VALUE;//开始解析参数值
            }
            else if (isblank(ch)) {
                m_eDecodeState = HttpRequestDecodeState::INVALID_URI;//无效的请求参数
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
                //收获一个请求参数
                m_mRequestParams.insert({ requestParamKey, requestParamValue });
                m_eDecodeState = HttpRequestDecodeState::BEFORE_URI_PARAM_KEY;
            }
            else if (isblank(ch)) {
                requestParamValue.m_pEnd = p;
                //空格也收获一个请求参数
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
            //解析请求协议
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
            //协议解析，如果不是数字或者. 就不对
            if (ch == CR) {
                version.m_pEnd = p;
                m_strVersion = version;
                m_eDecodeState = HttpRequestDecodeState::WHEN_CR;
            }
            else if (ch == '.') {
                //遇到版本分割
                m_eDecodeState = HttpRequestDecodeState::VERSION_SPLIT;
            }
            else if (isdigit(ch)) {
                //do nothing
            }
            else {
                m_eDecodeState = HttpRequestDecodeState::INVALID_VERSION;//不能不是数字
            }
            break;
        }
        case HttpRequestDecodeState::VERSION_SPLIT: {
            //遇到版本分割符，字符必须是数字，其他情况都是错误
            if (isdigit(ch)) {
                m_eDecodeState = HttpRequestDecodeState::VERSION;
            }
            else {
                m_eDecodeState = HttpRequestDecodeState::INVALID_VERSION;
            }
            break;
        }
        case HttpRequestDecodeState::HEADER_KEY: {
            //冒号前后可能有空格
            if (isblank(ch)) {
                headerKey.m_pEnd = p;
                m_eDecodeState = HttpRequestDecodeState::HEADER_BEFORE_COLON;//冒号之前的状态
            }
            else if (ch == ':') {
                headerKey.m_pEnd = p;
                m_eDecodeState = HttpRequestDecodeState::HEADER_AFTER_COLON;//冒号之后的状态
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
                //冒号之前的状态不能是空格之外的其他字符
                m_eDecodeState = HttpRequestDecodeState::INVALID_HEADER;
            }
            break;
        }
        case HttpRequestDecodeState::HEADER_AFTER_COLON: {
            if (isblank(ch)) {//值之前遇到空格都是正常的
                //do nothing
            }
            else {
                headerValue.m_pBegin = p;
                m_eDecodeState = HttpRequestDecodeState::HEADER_VALUE;//开始处理值
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
                //如果是回车，可换成下一个
                m_eDecodeState = HttpRequestDecodeState::CR_LF;
            }
            else {
                m_eDecodeState = HttpRequestDecodeState::INVALID;
            }
            break;
        }
        case HttpRequestDecodeState::CR_LF: {
            if (ch == CR) {
                //如果在CR_LF状态之后还有CR，那么便是有点结束的味道了
                m_eDecodeState = HttpRequestDecodeState::CR_LF_CR;
            }
            else if (isblank(ch)) {
                m_eDecodeState = HttpRequestDecodeState::INVALID;
            }
            else {
                //如果不是，那么就可能又是一个请求头了，那就开始解析请求头
                headerKey.m_pBegin = p;
                m_eDecodeState = HttpRequestDecodeState::HEADER_KEY;
            }
            break;
        }
        case HttpRequestDecodeState::CR_LF_CR: {
            if (ch == LF) {
                //如果是\r接着\n 那么判断是不是需要解析请求体
                if (m_strHeaders.count("Content-Length") > 0) {
                    bodyLength = atoi(m_strHeaders["Content-Length"].c_str());
                    if (bodyLength > 0) {
                        m_eDecodeState = HttpRequestDecodeState::BODY;//解析请求体
                    }
                    else {
                        m_eDecodeState = HttpRequestDecodeState::COMPLETE;//完成了
                    }
                }
                else {
                    if (scanPos < size) {
                        bodyLength = size - scanPos;
                        m_eDecodeState = HttpRequestDecodeState::BODY;//解析请求体
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
            //解析请求体
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

// 文件操作符函数




// 静态变量初始化
int CHttp::m_iEpollFd = -1;
int CHttp::m_iHttpCnt = 0;

void CHttp::Init(int _sockfd, const sockaddr_in& _addr) {
    m_iSockFd = _sockfd;
    m_sAddr = _addr;

}