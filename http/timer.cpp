#include "timer.h"
#include <string.h>

CHeapTimer::CHeapTimer(int _delaytime) {
    this->m_iExpireTime = time(NULL) + _delaytime;
}


CTimerHeap::CTimerHeap(int _capacity):m_iCapacity(_capacity), m_iCurNum(0) {
    m_aTimers = new CHeapTimer*[m_iCapacity](); 
    if (!m_aTimers) throw std::exception();
}

CTimerHeap::~CTimerHeap() {
    delete [] m_aTimers;
    for (int i = 0; i < m_iCurNum; ++i) m_aTimers[i] = nullptr;
}


void CTimerHeap::HeapDown(int heap_node) {
    CHeapTimer* tmp_timer = m_aTimers[heap_node];

    int child = 0;
    for (; heap_node * 2 + 1 < m_iCurNum; heap_node = child) {
        child = heap_node * 2 + 1;
        if (child + 1 < m_iCurNum && m_aTimers[child + 1]->m_iExpireTime < m_aTimers[child]->m_iExpireTime) child++;
        if (tmp_timer->m_iExpireTime > m_aTimers[child]->m_iExpireTime) {
            m_aTimers[heap_node] = m_aTimers[child];
        }
        else break;
    }
    m_aTimers[heap_node] = tmp_timer;
}

void CTimerHeap::AddTimer(CHeapTimer* add_timer) {
    if (!add_timer) return;
    if (m_iCurNum >= m_iCapacity) Resize();
    
    int last_node = m_iCurNum++;
    int parent = 0;

    // 新加入的节点在最后，向上调整
    for( ; last_node > 0; last_node = parent ) {
        parent = ( last_node - 1 ) / 2;
        if( m_aTimers[parent]->m_iExpireTime > add_timer->m_iExpireTime ) {
            m_aTimers[last_node] = m_aTimers[parent];
        } 
        else {
            break;
        }
    }
    m_aTimers[last_node] = add_timer;
}

void CTimerHeap::DelTimer( CHeapTimer* del_timer ) {
    if( !del_timer ) return;
    
    del_timer->callback_func = nullptr;
}

void CTimerHeap::PopTimer() {
    if(!m_iCurNum) return;
    if(m_aTimers[0]) {
        m_aTimers[0] = m_aTimers[--m_iCurNum];
        delete m_aTimers[m_iCurNum];
        m_aTimers[m_iCurNum] = nullptr;
        HeapDown(0);  // 对新的根节点进行下滤
    }
}

void CTimerHeap::Tick() {
    CHeapTimer* tmp_timer = m_aTimers[0];
    time_t cur_time = time(NULL);
    LOG_INFO("timer tick %d", cur_time);
    while(m_iCurNum) {
        if(!tmp_timer) break;
        if(tmp_timer->m_iExpireTime > cur_time) break;
        if(m_aTimers[0]->callback_func) {
            m_aTimers[0]->callback_func(m_aTimers[0]->m_sClientData);
        }
        PopTimer();
        LOG_INFO("pop timer");
        tmp_timer = m_aTimers[0];
    }
}

void CTimerHeap::Resize() {
    CHeapTimer** tmp = new CHeapTimer*[m_iCapacity * 2]();
    if(!tmp) throw std::exception();
    memcpy(tmp, m_aTimers, m_iCurNum);
    m_iCapacity *= 2;
    delete[] m_aTimers;
    m_aTimers = tmp;
}

CHeapTimer* CTimerHeap::GetTop(){
    if (m_iCurNum != 0) {
        return m_aTimers[0];
    }
    return nullptr;
}