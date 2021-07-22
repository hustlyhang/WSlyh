#include "timer.h"
#include <iostream>



void cb_f(client_data* _client_data){
    printf("timer sock_fd is: %d, delay time is: %ld\n", _client_data->sock_fd, _client_data->timer->expire_time);
    return;
}

class CUnitTest {
public:
    CUnitTest();
    ~CUnitTest();
    void test();
    int gettop();
    void init();
private:
    TimerHeap* m_timer_heap;
    int* delay_time;
    int test_size;
};

CUnitTest::CUnitTest() {
    printf("construct!\n");
    init();
}
CUnitTest::~CUnitTest() {
    delete []delay_time;
    delete m_timer_heap;
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
void CUnitTest::test() {
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




int main() {
    //CUnitTest* m_test = new CUnitTest();
    CUnitTest m_test2;
    printf("begin test!\n");
    //m_test->test();
    m_test2.test();
    
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