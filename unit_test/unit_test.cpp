#include "../http/timer.h"
#include <iostream>
#include "../log/log.h"


void cb_f(client_data* _client_data){
    printf("timer sock_fd is: %d, delay time is: %ld\n", _client_data->sock_fd, _client_data->timer->expire_time);
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
private:
    TimerHeap* m_timer_heap = nullptr;

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
    m_timer_heap = new TimerHeap(test_size);
    delay_time = new int[test_size]();
    for (int i = 0; i < test_size; ++i) {
        std::cin>>delay_time[i];
    }
}
int CUnitTest::gettop() {
    return m_timer_heap->getTop()->m_client_data->sock_fd;
}
void CUnitTest::test_timer() {
    init();
    sockaddr_in addr_tmp;
    // 先生成10个client_data
    printf("in test\n");
    for (int i = 0; i < test_size; ++i) {
        printf("now is add %d\n", i);
        client_data* tmp_data = new client_data();
        HeapTimer* tmp_timer = new HeapTimer(delay_time[i]);

        tmp_timer->callback_func = cb_f;
        tmp_timer->m_client_data = tmp_data;
        
        tmp_data->address = addr_tmp;
        tmp_data->sock_fd = i;
        tmp_data->timer = tmp_timer;
        
        m_timer_heap->add_timer(tmp_timer);
    }
    while (m_timer_heap->getTop()) {
        printf("test loop!!!!!!\n");
        m_timer_heap->tick();
        sleep(1);
    }
    printf("test over!!!!!!\n");
}


/*****************log测试********************************/
void CUnitTest::test_log() {
    UTC_Time m_sUtc;
    m_sUtc.GetCurTime_debug();
}



int main() {
    CUnitTest m_test2;
    printf("begin test!\n");
    // m_test2.test_timer();
    m_test2.test_log();
    
    return 0;
}

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