#include "../http/timer.h"
#include <iostream>
#include "../log/log.h"
#include "../http/http.h"

void cb_f(SClientData* _client_data){
    printf("timer sock_fd is: %d, delay time is: %ld\n", _client_data->sock_fd, _client_data->timer->m_iExpireTime);
    return;
}

class CUnitTest {
public:
    CUnitTest();
    ~CUnitTest();
    void test_timer();
    void test_log();
    int gettop();
    void init();
    void CharArray(const char*);
    void HttpTest();
public:
    const char m_aLogDir[1024] = {"./ServerLog"};  //日志记录文件夹
private:
    CTimerHeap* m_timer_heap = nullptr;
    int* delay_time = nullptr;
    int test_size;
};

CUnitTest::CUnitTest() {
    printf("construct!\n");
}
CUnitTest::~CUnitTest() {
    if (delay_time)
        delete[]delay_time;
    delay_time = nullptr;
    if (m_timer_heap)
        delete m_timer_heap;
    m_timer_heap = nullptr;
}

void CUnitTest::init() {
    std::cout<<"please enter test size :"<<std::endl;
    std::cin>>test_size;
    m_timer_heap = new CTimerHeap(test_size);
    delay_time = new int[test_size]();
    for (int i = 0; i < test_size; ++i) {
        std::cin>>delay_time[i];
    }
}
int CUnitTest::gettop() {
    return m_timer_heap->GetTop()->m_sClientData->sock_fd;
}
void CUnitTest::test_timer() {
    init();
    sockaddr_in addr_tmp;
    // 先生成10个client_data
    printf("in test\n");
    for (int i = 0; i < test_size; ++i) {
        printf("now is add %d\n", i);
        SClientData* tmp_data = new SClientData();
        CHeapTimer* tmp_timer = new CHeapTimer(delay_time[i]);

        tmp_timer->callback_func = cb_f;
        tmp_timer->m_sClientData = tmp_data;
        
        tmp_data->address = addr_tmp;
        tmp_data->sock_fd = i;
        tmp_data->timer = tmp_timer;
        
        m_timer_heap->AddTimer(tmp_timer);
    }
    while (m_timer_heap->GetTop()) {
        printf("test loop!!!!!!\n");
        m_timer_heap->Tick();
        sleep(1);
    }
    printf("test over!!!!!!\n");
}


void CUnitTest::CharArray(const char* _charArray) {
    int pos = 0;
    while (_charArray[pos]) {
        std::cout << _charArray[pos] << std::endl;
        // printf("%c ", _charArray[pos]);
        pos++;
    }
    printf("this len is: %d\n", pos);

}
void CUnitTest::HttpTest() {
    // 测试http的网址解析是否正确
    CHttp m_cHttp;
    //FILE* fp = fopen("E:/WSlyh/url.txt", "r");
    //if (fp == nullptr) LOG_INFO("文件打开失败");
    char *str = { R"(POST /OneCollector/1.0 HTTP/1.1
Host: browser.events.data.msn.com
Connection: keep-alive
Content-Length: 3002
User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/92.0.4515.107 Safari/537.36 Edg/92.0.902.62
Content-Type: text/plain;charset=UTF-8
Accept: */*
Origin: https://ntp.msn.cn
Accept-Encoding: gzip, deflate, br
Accept-Language: zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6
Cookie: MUID=231480B5CA276512066590D5CB17641E)" };
    LOG_INFO("%s", str);
    char _test[10] = "123456789";
    for (int i = 0; i < 10; ++i) {
        char tmp = _test[i];
        LOG_INFO("%c", tmp);
    }
    int len = 1253;
    //fread(str, 1, 1024, fp);
    m_cHttp.Test(str, len);
    m_cHttp.Show();
    // 没有回车符，测试失败

}
/*****************log测试********************************/
void CUnitTest::test_log() {
    UTC_Time m_sUtc;
    m_sUtc.GetCurTime_debug();
    CRingLog* log = CRingLog::GetInstance();
    LOG_INIT(m_aLogDir, "WebServer", (int)LOG_LEVEL::TRACE); 
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t start_ts = (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
    for (int i = 0; i < 1e8; ++i){
        LOG_ERROR("my number is number my number is my number is my number is my number is my number is my number is %d", i);
    }
    gettimeofday(&tv, NULL);
    uint64_t end_ts = (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
    printf("time use %lums\n", end_ts - start_ts);
}


#if 0


int main() {
    CUnitTest m_test2;
    // m_test2.test_timer();
    m_test2.test_log();
    // m_test2.CharArray("this is a test     ");
    // m_test2.HttpTest();

    return 0;
}

#endif // DEBUG



/*
test:
    [8 10 4 6 7 12 3 9 1 2 14 12 15 16 19]
    [0 1  2 3 4 5  6 7 8 9 10 11 12 13 14]
结果[8 9  6 2 3 4  0 7 1 11 5 10 12 13 14]

*/

/*
    出现了个问题，如果在main函数中写
    ⅠCUnitTest m_test();
    就无法初始化，而且后面的m_test.test()在编译的时候会报错
    改成
    ⅡCUnitTest* m_test = new CUnitTest();
    就没问题了

    第一种是在栈区申请空间，第二种是在堆区申请空间
    因此第一种不需要自己去管理对象的销毁，第二中需要自己去调用析构函数

    应该是因为写了括号需要调用含参构造函数，但是没有?
*/ 



/*
    打印时间的时候长度不是根据字符来的吗？
    中文需要占用三个char？
    查到了
    UTF-8编码中，一个英文字符等于一个字节，一个中文（含繁体）等于三个字节。
    GCC中的参数n表示向str中写入n个字符，包括'\0'字符，并且返回实际的字符串长度。
*/