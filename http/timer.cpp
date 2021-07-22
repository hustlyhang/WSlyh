#include "timer.h"
#include <string.h>

HeapTimer::HeapTimer(int delaytime) {
    this->expire_time = time(NULL) + delaytime;
}


TimerHeap::TimerHeap(int _capacity):m_capacity(_capacity), m_cur_num(0) {
    m_timer_array = new HeapTimer*[m_capacity](); 
    if (!m_timer_array) throw std::exception();
}

TimerHeap::~TimerHeap() {
    for (int i = 0; i < m_cur_num; ++i) m_timer_array[i] = nullptr;
    delete [] m_timer_array;
}


void TimerHeap::heap_down(int heap_node) {
    HeapTimer* tmp_timer = m_timer_array[heap_node];

    int child = 0;
    for (; heap_node * 2 + 1 < m_cur_num; heap_node = child) {
        child = heap_node * 2 + 1;
        if (child + 1 < m_cur_num && m_timer_array[child + 1]->expire_time < m_timer_array[child]->expire_time) child++;
        if (tmp_timer->expire_time > m_timer_array[child]->expire_time) {
            m_timer_array[heap_node] = m_timer_array[child];
        }
        else break;
    }
    m_timer_array[heap_node] = tmp_timer;
}

void TimerHeap::add_timer(HeapTimer* add_timer) {
    if (!add_timer) return;
    if (m_cur_num >= m_capacity) resize();
    
    int last_node = m_cur_num++;
    int parent = 0;

    // 新加入的节点在最后，向上调整
    for( ; last_node > 0; last_node = parent ) {
        parent = ( last_node - 1 ) / 2;
        if( m_timer_array[parent]->expire_time > add_timer->expire_time ) {
            m_timer_array[last_node] = m_timer_array[parent];
        } 
        else {
            break;
        }
    }
    m_timer_array[last_node] = add_timer;
}

void TimerHeap::del_timer( HeapTimer* del_timer ) {
    if( !del_timer ) return;
    
    del_timer->callback_func = nullptr;
}

void TimerHeap::pop_timer() {
    if(!m_cur_num) return;
    if(m_timer_array[0]) {
        m_timer_array[0] = m_timer_array[m_cur_num - 1];
        m_timer_array[--m_cur_num] = nullptr;
        heap_down(0);  // 对新的根节点进行下滤
    }
}

void TimerHeap::tick() {
    HeapTimer* tmp_timer = m_timer_array[0];
    time_t cur_time = time(NULL);
    printf("now time is %ld\n", cur_time);
    while(m_cur_num) {
        printf("loop\n");
        if(!tmp_timer) break;
        printf("timer time is %ld\n", tmp_timer->expire_time);
        if(tmp_timer->expire_time > cur_time) break;
        if(m_timer_array[0]->callback_func) {
            printf("call_back\n");
            m_timer_array[0]->callback_func(m_timer_array[0]->m_client_data);
        }
        pop_timer();
        tmp_timer = m_timer_array[0];
    }
}

void TimerHeap::resize() {
    HeapTimer** tmp = new HeapTimer*[m_capacity * 2]();
    if(!tmp) throw std::exception();
    memcpy(tmp, m_timer_array, m_cur_num);
    m_capacity *= 2;
    delete[] m_timer_array;
    m_timer_array = tmp;
}

HeapTimer* TimerHeap::getTop(){
    if (m_cur_num != 0) {
        return m_timer_array[0];
    }
    return nullptr;
}