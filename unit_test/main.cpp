#include "../lock/locker.h"
#include "../threadpool/threadpool.h"
#include <unistd.h>

class test {
public:
    test(int _index):index(_index){};
    void run() {
        printf("第%d个task %p\n", index, this);
    }
    int index = 0;
};

int main() {
    threadpool<test> *m_pool = new threadpool<test>(16);
    printf("testing!!!!!!!!!!!!!!!\n");
    int i = 0;
    while (1)
    {
        ++i;
        test *tmp_test = new test(i);
        m_pool->append(tmp_test);
        // sleep(1);
    }
    
    return 0;
}