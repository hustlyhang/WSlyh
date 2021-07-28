#include "log.h"
#include <sys/syscall.h>	//system call
#include <errno.h>
#include <unistd.h>//access, getpid
#include <assert.h>//assert
#include <stdarg.h>//va_list
#include <sys/stat.h>//mkdir

pid_t GetPid() {
	return syscall(__NR_gettid);
}

// ��̬������ʼ��
CLocker CRingLog::m_cLocker;
CCond CRingLog::m_cCond;
uint32_t CRingLog::m_uOneBuffLen = 30 * 1024 * 1024;//30MB


CRingLog::CRingLog() :m_iBuffCnt(3), m_pCurBuf(nullptr), m_pPrstBuf(nullptr), m_pFp(nullptr), 
						m_iLogCnt(0), m_bEnv(false), m_iLevel(0), m_uLastLogTime(0), m_sTM() {
	// ����˫����
    CBufferCell* head = new CBufferCell(m_uOneBuffLen);
    if (!head)
    {
        fprintf(stderr, "no space to allocate cell_buffer\n");
        exit(1);
    }
    CBufferCell* current;
    CBufferCell* prev = head;

    for (int i = 1; i < m_iBuffCnt; ++i)
    {
        current = new CBufferCell(m_uOneBuffLen);
        if (!current)
        {
            fprintf(stderr, "no space to allocate cell_buffer\n");
            exit(1);
        }
        current->m_pPre = prev;
        prev->m_pNext = current;
        prev = current;
    }
    prev->m_pNext = head;
    head->m_pPre = prev;

    // ����ǰָ��ͳ־û�ָ�붼ָ��˫ѭ�������ͷ�ڵ�
    m_pCurBuf = head;
    m_pPrstBuf = head;

    m_Pid = GetPid();
}
