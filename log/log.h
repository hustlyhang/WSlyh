#ifndef LOG_H
#define LOG_H
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>//getpid, gettid
#include <iostream>
#include "../lock/locker.h"

#define DEBUG_TEST
#define MAX_PROG_NAME_LEN 128
#define MAX_LOG_DIR_LEN 512



// 日志等级
enum class LOG_LEVEL {
	FATAL = 1,
	ERROR,
	WARN,
	INFO,
	DEBUG,
	TRACE,
};

// 优化获取时间
struct UTC_Time {
	UTC_Time() {
		// 初始化时将时间信息保存下来
		struct timeval tt;
		gettimeofday(&tt, NULL);
		UpdateTime(tt);
		AddTimeAll();
	}
	uint64_t GetCurTime(int* p_msec = NULL) {
		struct timeval tv;
		gettimeofday(&tv, NULL);

		if (p_msec)
			*p_msec = tv.tv_usec / 1000;
		// 判断秒数和分钟数是否相同
		if (tv.tv_sec != m_cache_sec) {
			m_cache_sec = (uint64_t)tv.tv_sec;
			if (m_cache_sec / 60 == m_cache_min) {
				m_sec = m_cache_sec % 60;
				AddTimeSec();
			}
			else {
				// 分钟数也不一样了，直接全部更新
				UpdateTime(tv);
				AddTimeAll();
			}
		}
		return m_cache_sec;
	}

	char m_aTimeCache[20];
	int m_year, m_month, m_day, m_hour, m_minute, m_sec;

#ifdef DEBUG_TEST
	void GetCurTime_debug() {
		std::cout << m_aTimeCache << std::endl;
		AddTimeSec();
		std::cout << m_aTimeCache << std::endl;
	}
#endif // DEBUG

private:
	uint64_t m_cache_sec, m_cache_min;      // 用来保存待比较的秒和分，如果相等就直接用缓存，不相等再更新
	// 将所有的时间更新后放入缓冲区
	void AddTimeAll() {
		snprintf(m_aTimeCache, 20, "%04d-%02d-%02d %02d:%02d:%02d", m_year, m_month, m_day, m_hour, m_minute, m_sec);
	}
	// 只改变秒数，更新缓冲区里面的秒数即可
	void AddTimeSec() {
		snprintf(m_aTimeCache + 17, 3, "%02d", m_sec);
	}
	void UpdateTime(struct timeval& tt) {
		m_cache_sec = tt.tv_sec;
		m_cache_min = m_cache_sec / 60;
		struct tm cur_tm;
		localtime_r((time_t*)&m_cache_sec, &cur_tm);
		m_year = cur_tm.tm_year + 1900;
		m_month = cur_tm.tm_mon + 1;
		m_day = cur_tm.tm_mday;
		m_hour = cur_tm.tm_hour;
		m_minute = cur_tm.tm_min;
		m_sec = cur_tm.tm_sec;
	}
};

// 内存块，用于存储日志
class CBufferCell {
public:
	CBufferCell(uint32_t _len): m_eStatus(BUFFER_STATUS::FREE), m_pPre(nullptr), m_pNext(nullptr), m_uTolLength(_len), m_uCurLength(0){
		m_data = new char[m_uTolLength]();
		if (!m_data) {
			fprintf(stderr, "Have no enough memory!!!!!!!!\n");
			exit(1);
		}
	}
	CBufferCell(const CBufferCell&) = delete;
	CBufferCell& operator = (const CBufferCell&) = delete;

	uint32_t GetFreeLen() {
		return m_uTolLength - m_uCurLength;
	}

	bool IsEmpty() {
		return m_uCurLength == 0;
	}

	void Append(const char* _log_data, uint32_t _len) {
		if (GetFreeLen() < _len) return;
		memcpy(m_data + m_uCurLength, _log_data, (size_t)_len);
		m_uCurLength += _len;
	}

	void ClearBuffer() {
		m_uCurLength = 0;
		memset(m_data, '\0', m_uTolLength);
		m_eStatus = BUFFER_STATUS::FREE;
	}

	void Persist(FILE* _fp) {
		uint32_t tmpWriteLen = fwrite(m_data, 1, m_uCurLength, _fp);
		if (tmpWriteLen != m_uCurLength) {
			fprintf(stderr, "Write file fail!!!!!!!  write len : %d\n", tmpWriteLen);
		}
	}

public:
	enum class BUFFER_STATUS {
		FREE,
		FULL
	};
	BUFFER_STATUS m_eStatus;
	CBufferCell* m_pPre;
	CBufferCell* m_pNext;
private:
	char* m_data;
	uint32_t m_uTolLength;
	uint32_t m_uCurLength;
};

// 内存环实现日志，单例模式
class CRingLog {
public:
	static CRingLog* GetInstance() {
		static CRingLog rl;
		return &rl;
	}
	void InitLogPath(const char* _logdir, const char * _prog_name, int _level);
	LOG_LEVEL GetLevel() const { return m_eLevel; }
	void PersistLog();
	void TryAppendLog(const char* _lvl, const char* format, ...);

private:
	CRingLog();
	~CRingLog() {};
	CRingLog(const CRingLog&) = delete;
	CRingLog& operator = (const CRingLog&) = delete;
	bool DecisFile(int _year, int _mon, int _day);
public:


private:
	int m_iBuffCnt;			// 日志节点个数
	CBufferCell* m_pCurBuf, * m_pPrstBuf, * m_pLastBuf;
	FILE* m_pFp;
	pid_t m_Pid;
	int m_iYear, m_iMon, m_iDay;
	int m_iLogCnt;			// 当天日志数量
	char m_aProgName[MAX_PROG_NAME_LEN];
	char m_aLogDir[MAX_LOG_DIR_LEN];
	bool m_bEnv;		// 日记路径是否正确
	LOG_LEVEL m_eLevel;
	uint64_t m_uLastLogFailTime;		// 上一次日志记录失败的时间
	UTC_Time m_sTM;
	static CLocker m_cLocker;
	static CCond m_cCond;
	static uint32_t m_uOneBuffLen;	// 单个日志节点最大长度
};


#endif