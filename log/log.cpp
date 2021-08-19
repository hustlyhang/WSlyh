#include "log.h"
#include <sys/syscall.h>	//system call
#include <errno.h>
#include <unistd.h>     //access, getpid
#include <assert.h>     //assert
#include <stdarg.h>     //va_list
#include <sys/stat.h>   //mkdir
#include <sys/shm.h>    //for shmxxx
#include <sys/types.h>  //for ipc key

#define ONELOGFILELIMIT (1u * 1024 * 1024 * 1024)
#define LOGFILEMEMLIMIT (3u * 1024 * 1024 * 1024)
#define ONELOGLEN (4 * 1024) //4K
#define RELOGTHRESOLD 5
#define LOGWAITTIME 1       // ���־û�ʧ��ʱ�ȴ��źŵ�ʱ��
#define SHMKEY 500234       // �����ڴ�key

const char* curexelink = "/proc/self/exe";

pid_t GetPid() {
	return syscall(__NR_gettid);
}

// �ڹ����ڴ��д���CellBuffer
CBufferCell* CreateCellBuffer(key_t _shmKey, uint32_t _dataLen) {
    // ���������ڴ��
    int shmId = shmget(_shmKey, sizeof(CBufferCell) + _dataLen, IPC_CREAT | IPC_EXCL | 0666);
    if (shmId == -1 && errno == EEXIST)
        shmId = shmget(_shmKey, sizeof(CBufferCell) + _dataLen, 0666);
    if (shmId == -1) {
        perror("shmget wrong\n");
        return nullptr;
    }
    CBufferCell* bc = (CBufferCell*)shmat(shmId, 0, 0);
    *bc = CBufferCell(shmId, _dataLen);
    bc->m_aData = (char*)bc + sizeof(CBufferCell);
    return bc;
}

/*
    ����֮ǰ�󶨵Ĺ����ڴ棬ͬʱ����shmid
    ���ݵ�ǰ���еĳ����Լ��Լ��趨��һ��ֵ������Ψһshmkey
    ���Ψһ��shmkey���Ի�ȡ��һ�鹲���ڴ棬�ڴ�������Ļ�ȡcellbuffer��shmid
    ��ȡ�����id��Ϳ��Ա���������״��־
*/ 
int GetPrstCBShmId() {
    char exePatch[4069] = {};
    int cnt = readlink(curexelink, exePatch, 4069);
    if (cnt < 0 || cnt > 4069) return -1;
    key_t shmKey = ftok(exePatch, SHMKEY);
    int shmId = shmget(shmKey, sizeof(int*), IPC_CREAT | IPC_EXCL | 0666);
    if (shmId == -1 && errno == EEXIST) {
        shmId = shmget(shmKey, sizeof(int*), 0666);
        // ��֮ǰ�󶨵Ĺ����ڴ���
        int* CBShmId = (int*)shmat(shmId, 0, 0);
        int headCBShmId = *CBShmId;
        int curCBShmId = headCBShmId;
        int nextCBShmId;
        do {
            CBufferCell* CB = (CBufferCell*)shmat(curCBShmId, 0, 0);
            if (!CB) {
                perror("shmat wrong\n");
                break;
            }
            nextCBShmId = CB->m_iNextShmId;
            shmdt((void*)CB);
            shmctl(curCBShmId, IPC_RMID, nullptr);
            curCBShmId = nextCBShmId;
        } while (headCBShmId != curCBShmId);

    }
    if (shmId == -1) perror("shmget wrong!\n");
    return shmId;
}

// ��̬������ʼ��
CLocker CRingLog::m_cLocker;
CCond CRingLog::m_cCond;
uint32_t CRingLog::m_uOneBuffLen = 30 * 1024 * 1024;//30MB ������־��Ԫ��С


CRingLog::CRingLog() :m_iBuffCnt(3), m_pCurBuf(nullptr), m_pPrstBuf(nullptr), m_pFp(nullptr), 
						m_iLogCnt(0), m_bEnv(false), m_eLevel((LOG_LEVEL)0), m_uLastLogFailTime(0), m_sTM(), m_pPrstBCShmId(nullptr){
	// ����˫����
    // ����CB���Ĺ����ڴ��ַ
    int shmId = GetPrstCBShmId();
    if (shmId == -1) exit(1);
    // �õ�ָ�����ڴ��ָ��
    m_pPrstBCShmId = (int*)shmat(shmId, 0, 0);
    CBufferCell* head = CreateCellBuffer(SHMKEY, m_uOneBuffLen);
    if (!head)
    {
        fprintf(stderr, "no space to allocate cell_buffer\n");
        exit(1);
    }
    CBufferCell* current;
    CBufferCell* prev = head;

    for (int i = 1; i < m_iBuffCnt; ++i)
    {
        current = CreateCellBuffer(SHMKEY + 1, m_uOneBuffLen);
        if (!current)
        {
            fprintf(stderr, "no space to allocate cell_buffer\n");
            exit(1);
        }
        current->m_pPre = prev;
        current->m_iPreShmId = prev->m_iShmId;
        prev->m_pNext = current;
        prev->m_iNextShmId = current->m_iShmId;
        prev = current;
    }
    prev->m_pNext = head;
    prev->m_iNextShmId = head->m_iShmId;
    head->m_pPre = prev;
    head->m_iPreShmId = prev->m_iShmId;

    // ����ǰָ��ͳ־û�ָ�붼ָ��˫ѭ�������ͷ�ڵ�
    m_pCurBuf = head;
    m_pPrstBuf = head;

    // ����
    *m_pPrstBCShmId = m_pPrstBuf->m_iShmId;

    m_Pid = GetPid();
}

// ��ʼ����־·��
void CRingLog::InitLogPath(const char* _logdir, const char* _prog_name, int _level) {
    strncpy(m_aLogDir, _logdir, MAX_LOG_DIR_LEN);
    strncpy(m_aProgName, _prog_name, MAX_PROG_NAME_LEN);

    mkdir(m_aLogDir, 0777);

    if (access(m_aLogDir, F_OK | W_OK) == -1)
        fprintf(stderr, "mkdir %s fail! error: %s\n", m_aLogDir, strerror(errno));
    else
        m_bEnv = true;
    if (_level > (int)LOG_LEVEL::TRACE)
        m_eLevel = LOG_LEVEL::TRACE;
    if (_level < (int)LOG_LEVEL::FATAL)
        m_eLevel = LOG_LEVEL::FATAL;
    m_eLevel = (LOG_LEVEL)_level;
}

// ����򿪵���־�ļ�
bool CRingLog::DecisFile(int _year, int _mon, int _day) {
    // ���ȼ�黷���Ƿ�ok
    if (!m_bEnv) {
        if (m_pFp) 
            fclose(m_pFp);
        // �ض���
        m_pFp = fopen("/null", "w");
        return m_pFp != nullptr;
    }
    // �����û�д��ļ�
    if (!m_pFp) {
        m_iYear = _year;
        m_iDay = _day;
        m_iMon = _mon;
        char logPath[1024] = {};
        sprintf(logPath, "%s/%s.%d%02d%02d.%u.log", m_aLogDir, m_aProgName, m_iYear, m_iMon, m_iDay, m_Pid);
        m_pFp = fopen(logPath, "w");
        if (m_pFp)
            m_iLogCnt++;
    }
    else if (m_iDay != _day) {
        // �������£��½���־�ļ�
        fclose(m_pFp);
        m_iDay = _day;
        char logPath[1024] = {};
        sprintf(logPath, "%s/%s.%d%02d%02d.%u.log", m_aLogDir, m_aProgName, m_iYear, m_iMon, m_iDay, m_Pid);
        m_pFp = fopen(logPath, "w");
        if (m_pFp)
            m_iLogCnt = 1;
    }
    else if (ftell(m_pFp) >= ONELOGFILELIMIT) {
        // ������־�ļ�̫��
        fclose(m_pFp);
        char old_path[1024] = {};
        char new_path[1024] = {};
        // ���ļ�������
        for (int i = m_iLogCnt - 1; i > 0; --i) {
            sprintf(old_path, "%s/%s.%d%02d%02d.%u.log.%d", m_aLogDir, m_aProgName, _year, _mon, _day, m_Pid, i);
            sprintf(new_path, "%s/%s.%d%02d%02d.%u.log.%d", m_aLogDir, m_aProgName, _year, _mon, _day, m_Pid, i + 1);
            rename(old_path, new_path);
        }
        sprintf(old_path, "%s/%s.%d%02d%02d.%u.log", m_aLogDir, m_aProgName, _year, _mon, _day, m_Pid);
        sprintf(new_path, "%s/%s.%d%02d%02d.%u.log.1", m_aLogDir, m_aProgName, _year, _mon, _day, m_Pid);
        rename(old_path, new_path);
        m_pFp = fopen(old_path, "w");
        if (m_pFp)
            m_iLogCnt ++;
    }
    return m_pFp != nullptr;
}

// ׷����־
/*
*   _lvl ��־�ȼ�
*   _format ��־��ʽ
*/
void CRingLog::TryAppendLog(const char* _lvl, const char* _format, ...) {
    // ������Ҫ��ȡʱ��
    int tMs = 0;   // ΢��
    uint64_t curSec = m_sTM.GetCurTime(&tMs);
    // �����ϴ������־ʧ�ܺ��ٴ������־��ʱ����
    if (m_uLastLogFailTime && curSec - m_uLastLogFailTime < RELOGTHRESOLD)
        return;

    char logLine[ONELOGLEN] = {};
    int preLen = snprintf(logLine, ONELOGLEN, "%s[%s.%03d]", _lvl, m_sTM.m_aTimeCache, tMs);

    va_list argPtr;
    va_start(argPtr, _format);

    int postLen = vsnprintf(logLine + preLen, ONELOGLEN - preLen, _format, argPtr);

    va_end(argPtr);

    uint32_t logLen = preLen + postLen;

    // ������־ʧ��ʱ��
    m_uLastLogFailTime = 0;
    // �������̵߳���Ϣ��־
    bool backFlag = false;

    // ������Ҫ��Ԫ��д��־����Ҫ����
    m_cLocker.lock();
    if (m_pCurBuf->m_eStatus == CBufferCell::BUFFER_STATUS::FREE && m_pCurBuf->GetFreeLen() >= logLen) {
        // ��ǰ��־��Ԫ�ǿ���״̬����ʣ�೤���㹻
        m_pCurBuf->Append(logLine, logLen);
        m_cLocker.unlock();
    }
    else {
        // ���Ȳ���
        if (m_pCurBuf->m_eStatus == CBufferCell::BUFFER_STATUS::FREE) {
            // �ȸ��ĵ�ǰ��Ԫ��״̬
            m_pCurBuf->m_eStatus = CBufferCell::BUFFER_STATUS::FULL;
            // ��ǰ�ƶ�
            CBufferCell* nextBuf = m_pCurBuf->m_pNext;
            // ��ǰ�����Ԫ���Գ־û��ˣ�����֪ͨ�����̣߳��޸ı�־״̬
            backFlag = true;
            // �����һ����Ԫ���־û�����ô˵�����ˣ�Ҫô���ݣ�Ҫô����
            if (nextBuf->m_eStatus == CBufferCell::BUFFER_STATUS::FULL) {
                // ��鵱ǰ����,�����˾ͱ���
                if (m_uOneBuffLen * (m_iBuffCnt + 1) > LOGFILEMEMLIMIT) {
                    fprintf(stderr, "no more log space can use\n");
                    m_pCurBuf = nextBuf;
                    m_uLastLogFailTime = curSec;
                }
                else {
                    // ����һ����Ԫ
                    CBufferCell* newBuffer = CreateCellBuffer(SHMKEY + m_iBuffCnt, m_uOneBuffLen);
                    m_iBuffCnt += 1;
                    newBuffer->m_pPre = m_pCurBuf;
                    newBuffer->m_iPreShmId = m_pCurBuf->m_iShmId;
                    m_pCurBuf->m_pNext = newBuffer;
                    m_pCurBuf->m_iNextShmId = newBuffer->m_iShmId;
                    newBuffer->m_pNext = nextBuf;
                    newBuffer->m_iNextShmId = nextBuf->m_iShmId;
                    nextBuf->m_pPre = newBuffer;
                    nextBuf->m_iPreShmId = newBuffer->m_iShmId;
                    m_pCurBuf = newBuffer;
                }
            }
            else {
                m_pCurBuf = nextBuf;
            }
            // �������ݵĻ���д��
            if (!m_uLastLogFailTime)
                m_pCurBuf->Append(logLine, logLen);
        }
        else {
            // �־û�ָ��Ҳ�����
            // ʧ�ܣ���¼ʧ��ʱ��
            m_uLastLogFailTime = curSec;
        }
        m_cLocker.unlock();
    }
    if (backFlag)
        m_cCond.signal();
}


// ������־
void CRingLog::PersistLog() {
    while (true) {
        // ��Ҫ����־���в���
        m_cLocker.lock();
        if (m_pPrstBuf->m_eStatus == CBufferCell::BUFFER_STATUS::FREE) {
            // ��ǰλ���ǿ��У�˵����û��д�룬��Ҫ�ȴ�
            struct timeval tTv;
            struct timespec tTs;
            gettimeofday(&tTv, NULL);
            tTs.tv_sec = tTv.tv_sec + LOGWAITTIME;
            tTs.tv_nsec = tTv.tv_usec * 1000;
            m_cCond.wait_time(m_cLocker.get(), tTs);
        }

        //  ��û�п�ʼд��־
        if (m_pPrstBuf->IsEmpty()) {
            m_cLocker.unlock();
            continue;
        }

        if (m_pPrstBuf->m_eStatus == CBufferCell::BUFFER_STATUS::FREE) {
            // �������free����ôֱ�Ӹ�Ϊfull����������
            // �ж�д��ָ��ͳ־û�ʱ���Ƿ���ͬһ������Ϊ������ǵĻ�����ǰ�־û�ָ����ָ���buffer�϶���FULL
            assert(m_pCurBuf == m_pPrstBuf);
            // ����ǰ״̬��ΪfullȻ��д��ָ�������ƶ�
            m_pCurBuf->m_eStatus = CBufferCell::BUFFER_STATUS::FULL;
            m_pCurBuf = m_pCurBuf->m_pNext;
        }

        int year = m_sTM.m_year;
        int mon = m_sTM.m_month;
        int day = m_sTM.m_day;
        m_cLocker.unlock();

        // �������־�ļ������ļ�ָ��Ϊ��
        if (!DecisFile(year, mon, day))
            continue;

        // ���־û�ָ��ָ�����־��Ԫ���г־û�
        m_pPrstBuf->Persist(m_pFp);
        fflush(m_pFp);

        m_cLocker.lock();
        m_pPrstBuf->ClearBuffer();
        m_pPrstBuf = m_pPrstBuf->m_pNext;
        *m_pPrstBCShmId = m_pPrstBuf->m_iShmId;
        m_cLocker.unlock();
    }
}


void* ConFunc(void* arg) {
    CRingLog::GetInstance()->PersistLog();
    return nullptr;
}

