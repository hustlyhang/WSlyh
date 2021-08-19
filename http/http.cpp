#include "http.h"
#include <string.h>
#include <vector>
#include <unistd.h> //fcntl close
#include <fcntl.h>  //fcntl
#include <sys/epoll.h>  // epoll
#include <sys/uio.h>    // writev
#include <stdarg.h> // va_start
#include <sys/mman.h>   // mmap

//定义http响应的一些状态信息
const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char* error_403_title = "Forbidden";
const char* error_403_form = "You do not have permission to get file form this server.\n";
const char* error_404_title = "Not Found";
const char* error_404_form = "The requested file was not found on this server.\n";
const char* error_500_title = "Internal Error";
const char* error_500_form = "There was an unusual problem serving the request file.\n";

// 结构体初始化
void SHttpRequest::Init() {
    m_eDecodeState = HttpRequestDecodeState::START;
    m_iNextPos = 0;
    m_bIsOpen = false;
    m_strBody.clear();
    m_mHeaders.clear();
    m_strMethod.clear();
    m_strProtocol.clear();
    m_strUrl.clear();
    m_strVersion.clear();
    m_mRequestParams.clear();
}
// 协议解析
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
                m_mHeaders.insert({ headerKey, headerValue });
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
                if (m_mHeaders.count("Content-Length") > 0) {
                    bodyLength = atoi(m_mHeaders["Content-Length"].c_str());
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

int SHttpRequest::GetLen() {
    return m_iNextPos;
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
    return m_mHeaders;
}

const std::string& SHttpRequest::GetBody() const {
    return m_strBody;
}

const HttpRequestDecodeState SHttpRequest::GetStatus() const {
    return m_eDecodeState;
}

// 文件操作符函数
// 文件描述符非阻塞
int SetNonBlocking(int _fd) {
    int oldOption = fcntl(_fd, F_GETFL);
    int newOption = oldOption | O_NONBLOCK;
    fcntl(_fd, F_SETFL, newOption);
    return oldOption;
}

// 向epoll中注册文件描述符
void AddFd(int _epollFd, int _fd, bool _oneShot, int _triggerMode) {
    epoll_event event;
    event.data.fd = _fd;
    if (_triggerMode == 1)
        event.events = EPOLLIN | EPOLLRDHUP | EPOLLET; // 启用ET模式
    else 
        event.events = EPOLLIN | EPOLLRDHUP;           // 启用LT模式

    if (_oneShot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(_epollFd, EPOLL_CTL_ADD, _fd, &event);
    
    SetNonBlocking(_fd);
}

void RemoveFd(int _epollFd, int _fd) {
    epoll_ctl(_epollFd, EPOLL_CTL_DEL, _fd, nullptr);
    close(_fd);
}

// 事件触发后，需要重置oneshot
void ModFd(int _epollFd, int _fd, int _event, int _triggerMode) {
    epoll_event event;
    event.data.fd = _fd;

    if (_triggerMode == 1)
        event.events = _event | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    else
        event.events = _event | EPOLLONESHOT | EPOLLRDHUP;

    epoll_ctl(_epollFd, EPOLL_CTL_MOD, _fd, &event);
}


// 静态变量初始化
int CHttp::m_iEpollFd = -1;
int CHttp::m_iHttpCnt = 0;
char CHttp::m_aFilePathPrefix[100] = "";

void CHttp::Init(int _sockfd, const sockaddr_in& _addr, int _triggerMode) {
    m_iSockFd = _sockfd;
    m_sAddr = _addr;
    m_iTriggerMode = _triggerMode;

    AddFd(m_iEpollFd, _sockfd, true, m_iTriggerMode);
    m_iHttpCnt++;
    
    m_iReadIdx = 0;
    m_iWriteIdx = 0;

    memset(m_aReadData, '\0', MAX_READ_DATA_BUFF_SIZE);
    memset(m_aWriteData, '\0', MAX_WRITE_DATA_BUFF_SIZE);

    m_sHttpParse.Init();
}

void CHttp::CloseHttp() {
    if (m_iSockFd == -1)
        return;
    LOG_INFO("close %d", m_iSockFd);
    RemoveFd(m_iEpollFd, m_iSockFd);
    m_iSockFd = -1;
    m_iHttpCnt--;
}

void CHttp::unmmap() {
    if (m_pFileAddr) {
        munmap(m_pFileAddr, m_sFileStat.st_size);
        m_pFileAddr = nullptr;
    }
}

bool CHttp::Read() {
    // 检查缓冲区是否溢出
    if (m_iReadIdx >= MAX_READ_DATA_BUFF_SIZE) {
        LOG_INFO("读缓冲区溢出\n");
        return false;
    }

    ssize_t recLen = 0;

    if (m_iTriggerMode) {
        // ET
        while (true) {
            // 需要一直读，直到错误是EAGAIN或EWOULDBLOCK才退出
            recLen = recv(m_iSockFd, m_aReadData + m_iReadIdx, MAX_READ_DATA_BUFF_SIZE - m_iReadIdx, 0);
            if (recLen == -1) {
                // 报错
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                return false;
            }
            else if (recLen == 0)
                return false;
            else m_iReadIdx += recLen;
        }
        return true;
    }
    else {
        recLen = recv(m_iSockFd, m_aReadData + m_iReadIdx, MAX_READ_DATA_BUFF_SIZE - m_iReadIdx, 0);
        if (recLen <= 0)
            return false;
        m_iReadIdx += recLen;
        return true;
    }
}

bool CHttp::Write() {

}

HttpRequestDecodeState CHttp::ParseRead() {
    // 处理读取到缓冲区的数据，如果不完整，返回false，并且继续监听可读事件
    m_sHttpParse.ParseInternal(m_aReadData, m_iReadIdx);
    HttpRequestDecodeState status = m_sHttpParse.GetStatus();
    LOG_INFO("处理http请求错误，当前处理机状态%d, 当前内容%s", (int)status, m_aReadData);
    if (m_sHttpParse.GetLen() == m_iReadIdx) {
        // 说明读到了末尾
        if (status != HttpRequestDecodeState::INVALID_VERSION &&
            status != HttpRequestDecodeState::INVALID &&
            status != HttpRequestDecodeState::INVALID_HEADER &&
            status != HttpRequestDecodeState::INVALID_METHOD &&
            status != HttpRequestDecodeState::INVALID_URI) {
            return HttpRequestDecodeState::OPEN;
        }
    }
    return m_sHttpParse.GetStatus();
}

bool CHttp::AddLine(const char* _format, ...) {
    if (m_iWriteIdx >= MAX_WRITE_DATA_BUFF_SIZE) {
        
        return false;
    }
    va_list arglist;
    va_start(arglist, _format);
    int len = vsnprintf(m_aWriteData + m_iWriteIdx, MAX_WRITE_DATA_BUFF_SIZE - 1 - m_iWriteIdx, _format, arglist);
    if (len >= MAX_WRITE_DATA_BUFF_SIZE - 1 - m_iWriteIdx) {
        // 长度超出限制
        va_end(arglist);
        LOG_ERROR("写缓冲区溢出");
        return false;
    }
    m_iWriteIdx += len;
    va_end(arglist);
    LOG_INFO("add line:%s", m_aWriteData + m_iWriteIdx - len);
    return true;
}

bool CHttp::AddStatusLine(int _status, const char* _title) {
    return AddLine("%s %d %s\r\n", "HTTP/1.1", _status, _title);
}
bool CHttp::AddHeaders(int _contentlen) {
    return AddContentLength(_contentlen) && AddLinger() &&
        AddBlankLine()&& AddContentType();
}
bool CHttp::AddContentLength(int _contentlen) {
    return AddLine("Content-Length:%d\r\n", _contentlen);
}
bool CHttp::AddContentType() {
    return AddLine("Content-Type:%s\r\n", "text/html");
}
bool CHttp::AddLinger() {
    return AddLine("Connection:%s\r\n", (m_bLinger == true) ? "keep-alive" : "close");
}
bool CHttp::AddBlankLine() {
    return AddLine("%s", "\r\n");
}
bool CHttp::AddContent(const char* _content) {
    return AddLine("%s", _content);
}

bool CHttp::ParseWrite(HttpRequestDecodeState _status) {
    // 处理玩家请求的数据，分析请求的结果是否正确，
    // 先返回默认的页面
    if (_status == HttpRequestDecodeState::INVALID_VERSION ||
        _status == HttpRequestDecodeState::INVALID ||
        _status == HttpRequestDecodeState::INVALID_HEADER ||
        _status == HttpRequestDecodeState::INVALID_METHOD ||
        _status == HttpRequestDecodeState::INVALID_URI) {
        // http请求出现问题
        AddStatusLine(400, error_400_title);
        AddHeaders(strlen(error_400_form));
        return AddContent(error_400_form);
    }
    else if (_status == HttpRequestDecodeState::COMPLETE) {
        // 解析完成
        
        // 先返回默认页面,这些判断后续会用到
        if (stat(m_aFile, &m_sFileStat) < 0) {
            LOG_ERROR("获取默认页面文件属性失败");
            AddStatusLine(404, error_404_title);
            AddHeaders(strlen(error_404_form));
            return AddContent(error_404_form);
        }
        if (!(m_sFileStat.st_mode & S_IROTH)) {
            AddStatusLine(403, error_403_title);
            AddHeaders(strlen(error_403_form));
            return AddContent(error_403_form);
        }
        // 判断请求的文件是不是目录
        if (S_ISDIR(m_sFileStat.st_mode)) {
            AddStatusLine(404, error_404_title);
            AddHeaders(strlen(error_404_form));
            return AddContent(error_404_form);
        }
        // 请求的文件没有问题，将文件映射到缓冲区中
        int fd = open(m_aFile, O_RDONLY);
        m_pFileAddr = (char*)mmap(0, m_sFileStat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        close(fd);
        AddStatusLine(200, ok_200_title);
        AddHeaders(m_sFileStat.st_size);
        m_sIv[0].iov_base = m_aWriteData;
        m_sIv[0].iov_len = m_iWriteIdx;
        m_sIv[1].iov_base = m_pFileAddr;
        m_sIv[1].iov_len = m_sFileStat.st_size;
        m_iIvCount = 2;
        m_iDateLen = m_iWriteIdx + m_sFileStat.st_size;
        return true;
    }
    else {
        AddStatusLine(500, error_500_title);
        AddHeaders(strlen(error_500_form));
        return AddContent(error_500_form);
    }
}

void CHttp::HttpParse() {
    HttpRequestDecodeState status = ParseRead();
    if (status == HttpRequestDecodeState::OPEN) {
        // 当还没有接收完数据时，需要重新监听
        ModFd(m_iEpollFd, m_iSockFd, EPOLLIN, m_iTriggerMode);
        return;
    }
    bool ret = ParseWrite(status);
    if (!ret) {
        // 说明请求中除了问题，关闭连接
        CloseHttp();
        return;
    }
    // 没有问题就改变m_iSockFd的监听事件
    ModFd(m_iEpollFd, m_iSockFd, EPOLLOUT, m_iTriggerMode);
}