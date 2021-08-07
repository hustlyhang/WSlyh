#include "log.h"
#include <sys/syscall.h>	//system call
#include <errno.h>
#include <unistd.h>//access, getpid
#include <assert.h>//assert
#include <stdarg.h>//va_list
#include <sys/stat.h>//mkdir

#define ONELOGFILELIMIT (1u * 1024 * 1024 * 1024)
#define LOGFILEMEMLIMIT (3u * 1024 * 1024 * 1024)
#define ONELOGLEN (4 * 1024) //4K
#define RELOGTHRESOLD 5
#define LOGWAITTIME 1       // 当持久化失败时等待信号的时间

pid_t GetPid() {
	return syscall(__NR_gettid);
}

// 静态变量初始化
CLocker CRingLog::m_cLocker;
CCond CRingLog::m_cCond;
uint32_t CRingLog::m_uOneBuffLen = 30 * 1024 * 1024;//30MB 单个日志单元大小


CRingLog::CRingLog() :m_iBuffCnt(3), m_pCurBuf(nullptr), m_pPrstBuf(nullptr), m_pFp(nullptr), 
						m_iLogCnt(0), m_bEnv(false), m_eLevel((LOG_LEVEL)0), m_uLastLogFailTime(0), m_sTM() {
	// 创建双链表
	CBufferCell* head = new CBufferCell(m_uOneBuffLen);
	if (!head) {
		fprintf(stderr, "no space to allocate cell_buffer\n");
		exit(1);
	}
	CBufferCell* current;
	CBufferCell* prev = head;

	for (int i = 1; i < m_iBuffCnt; ++i) {
		current = new CBufferCell(m_uOneBuffLen);
		if (!current) {
			fprintf(stderr, "no space to allocate cell_buffer\n");
			exit(1);
		}
		current->m_pPre = prev;
		prev->m_pNext = current;
		prev = current;
	}
	prev->m_pNext = head;
	head->m_pPre = prev;

	// 将当前指针和持久化指针都指向双循环链表的头节点
	m_pCurBuf = head;
	m_pPrstBuf = head;

	m_Pid = GetPid();
}

// 初始化日志路径
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

// 处理打开的日志文件
bool CRingLog::DecisFile(int _year, int _mon, int _day) {
	// 首先检查环境是否ok
	if (!m_bEnv) {
		if (m_pFp) 
			fclose(m_pFp);
		// 重定向
		m_pFp = fopen("/null", "w");
		return m_pFp != nullptr;
	}
	// 如果还没有打开文件
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
		// 天数更新，新建日志文件
		fclose(m_pFp);
		m_iDay = _day;
		char logPath[1024] = {};
		sprintf(logPath, "%s/%s.%d%02d%02d.%u.log", m_aLogDir, m_aProgName, m_iYear, m_iMon, m_iDay, m_Pid);
		m_pFp = fopen(logPath, "w");
		if (m_pFp)
			m_iLogCnt = 1;
	}
	else if (ftell(m_pFp) >= ONELOGFILELIMIT) {
		// 单个日志文件太大
		fclose(m_pFp);
		char old_path[1024] = {};
		char new_path[1024] = {};
		// 给文件重命名
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

// 追加日志
/*
*   _lvl 日志等级
*   _format 日志格式
*/
void CRingLog::TryAppendLog(const char* _lvl, const char* _format, ...) {
	// 首先需要获取时间
	int tMs = 0;   // 微秒
	uint64_t curSec = m_sTM.GetCurTime(&tMs);
	// 限制上次添加日志失败后再次添加日志的时间间隔
	if (m_uLastLogFailTime && curSec - m_uLastLogFailTime < RELOGTHRESOLD)
		return;

	char logLine[ONELOGLEN] = {};
	int preLen = snprintf(logLine, ONELOGLEN, "%s[%s.%03d]", _lvl, m_sTM.m_aTimeCache, tMs);

	va_list argPtr;
	va_start(argPtr, _format);

	int postLen = vsnprintf(logLine + preLen, ONELOGLEN - preLen, _format, argPtr);

	va_end(argPtr);

	uint32_t logLen = preLen + postLen;

	// 重置日志失败时间
	m_uLastLogFailTime = 0;
	// 给消费线程的信息标志
	bool backFlag = false;

	// 接下来要向单元内写日志，需要加锁
	m_cLocker.lock();
	if (m_pCurBuf->m_eStatus == CBufferCell::BUFFER_STATUS::FREE && m_pCurBuf->GetFreeLen() >= logLen) {
		// 当前日志单元是空闲状态并且剩余长度足够
		m_pCurBuf->Append(logLine, logLen);
		m_cLocker.unlock();
	}
	else {
		// 长度不够
		if (m_pCurBuf->m_eStatus == CBufferCell::BUFFER_STATUS::FREE) {
			// 先更改当前单元的状态
			m_pCurBuf->m_eStatus = CBufferCell::BUFFER_STATUS::FULL;
			// 往前移动
			CBufferCell* nextBuf = m_pCurBuf->m_pNext;
			// 当前这个单元可以持久化了，可以通知消费线程，修改标志状态
			backFlag = true;

			// 如果下一个单元待持久化，那么说明满了，要么扩容，要么报错
			if (nextBuf->m_eStatus == CBufferCell::BUFFER_STATUS::FULL) {
				// 检查当前容量,超过了就报错
				if (m_uOneBuffLen * (m_iBuffCnt + 1) > LOGFILEMEMLIMIT) {
					fprintf(stderr, "no more log space can use\n");
					m_pCurBuf = nextBuf;
					m_uLastLogFailTime = curSec;
				}
				else {
					// 扩增一个单元
					CBufferCell* newBuffer = new CBufferCell(m_uOneBuffLen);
					m_iBuffCnt += 1;
					newBuffer->m_pPre = m_pCurBuf;
					m_pCurBuf->m_pNext = newBuffer;
					newBuffer->m_pNext = nextBuf;
					nextBuf->m_pPre = newBuffer;
					m_pCurBuf = newBuffer;
				}
			}
			else {
				m_pCurBuf = nextBuf;
			}
			if (!m_uLastLogFailTime)
				m_pCurBuf->Append(logLine, logLen);
		}
		else {
			// 持久化指针也在这儿
			// 失败，记录失败时间
			m_uLastLogFailTime = curSec;
		}
		m_cLocker.unlock();
	}
	if (backFlag)
		m_cCond.signal();
}


// 保存日志
void CRingLog::PersistLog() {
	while (true) {
		// 需要对日志进行操作
		m_cLocker.lock();
		if (m_pPrstBuf->m_eStatus == CBufferCell::BUFFER_STATUS::FREE) {
			// 当前位置是空闲，说明还没有写入，需要等待
			struct timeval tTv;
			struct timespec tTs;
			gettimeofday(&tTv, NULL);
			tTs.tv_sec = tTv.tv_sec + LOGWAITTIME;
			tTs.tv_nsec = tTv.tv_usec * 1000;
			m_cCond.wait_time(m_cLocker.get(), tTs);
		}

		if (m_pPrstBuf->IsEmpty()) {
			m_cLocker.unlock();
			continue;
		}

		if (m_pPrstBuf->m_eStatus == CBufferCell::BUFFER_STATUS::FREE) {
			// 如果还是free，那么直接改为full，继续往下
			// 判断写入指针和持久化时针是否是同一个
			assert(m_pCurBuf == m_pPrstBuf);
			// 将当前状态改为full然后将写入指针往下移动
			m_pCurBuf->m_eStatus = CBufferCell::BUFFER_STATUS::FULL;
			m_pCurBuf = m_pCurBuf->m_pNext;
		}

		int year = m_sTM.m_year;
		int mon = m_sTM.m_month;
		int day = m_sTM.m_day;
		m_cLocker.unlock();

		// 如果打开日志文件有误，文件指针为空
		if (!DecisFile(year, mon, day))
			continue;

		// 将持久化指针指向的日志单元进行持久化
		m_pPrstBuf->Persist(m_pFp);
		fflush(m_pFp);

		m_cLocker.lock();
		m_pPrstBuf->ClearBuffer();
		m_pPrstBuf = m_pPrstBuf->m_pNext;
		m_cLocker.unlock();
	}
}


void* ConFunc(void* arg) {
	CRingLog::GetInstance()->PersistLog();
	return nullptr;
}

