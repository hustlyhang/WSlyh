// test syn

#include "../log.h"
#include <unistd.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>
#include <fstream>
#include <pthread.h>

const long long cnt = 1e7;
const long long mcnt = 1e6;
std::string fflog("test test test test test test test test test test test test test test test test test test test test.");
constexpr int N = 4;
std::ofstream ofs;

CLocker lock;

int64_t get_current_millis(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

void OneThreadAsyn(long long cnt) {
    LOG_INIT("./PipeLog.log", "test", 3);
    uint64_t start_ts = get_current_millis();
    for (int i = 0;i < cnt; ++i) {
        LOG_ERROR("test test test test test test test test test test test test test test test test test test test test.");
    }
    uint64_t end_ts = get_current_millis();
    printf("Asyn time use %lums\n", end_ts - start_ts);
    printf("Write log per second %lu MB\n", (cnt * fflog.size() / 2^10) /(end_ts - start_ts));
}
void OneThreadSyn(long long cnt) {
    uint64_t start_ts = get_current_millis();
    for (int i = 0; i < cnt; ++i) {
        time_t t = time(0);
        char tmp[64];
        strftime(tmp, sizeof(tmp), "[%Y-%m-%d %X]", localtime(&t));  
        ofs << tmp << " - ";
        ofs << "test test test test test test test test test test test test test test test test test test test test." <<std::endl; 
    }
    uint64_t end_ts = get_current_millis();
    printf("Syn time use %lums\n", end_ts - start_ts);
    printf("Write log per second %lu MB\n", (cnt * fflog.size() / 2^10) /(end_ts - start_ts));
}

void* thdoasyn(void* args)
{
    for (int i = 0;i < mcnt; ++i) {
        LOG_ERROR("test test test test test test test test test test test test test test test test test test test test.");
    }
}

void* thdosyn(void* args)
{
    for (int i = 0;i < mcnt; ++i) {
        time_t t = time(0);
        char tmp[64];
        strftime(tmp, sizeof(tmp), "[%Y-%m-%d %X]", localtime(&t));  
        lock.Lock();
        ofs << tmp << " - ";
        ofs << "test test test test test test test test test test test test test test test test test test test test." <<std::endl; 
        lock.Unlock();
    }
}

void MulThreadAsyn() {
    LOG_INIT("./PipeLog.log", "test", 3);
    uint64_t start_ts = get_current_millis();
    pthread_t tids[N];
    for (int i = 0;i < N; ++i) {
        pthread_create(&tids[i], NULL, thdoasyn, NULL);
        pthread_join(tids[i], NULL);
    }
    uint64_t end_ts = get_current_millis();
    printf("MultiAsyn time use %lums\n", end_ts - start_ts);
    printf("Write log per second %lu MB\n", (cnt * fflog.size() / 2^10) /(end_ts - start_ts));
}

void MulThreadSyn() {
    
    uint64_t start_ts = get_current_millis();
    pthread_t tids[N];
    for (int i = 0;i < N; ++i){
        pthread_create(&tids[i], NULL, thdosyn, NULL);
        pthread_join(tids[i], NULL);
    }
    uint64_t end_ts = get_current_millis();
    printf("MultiSyn time use %lums\n", end_ts - start_ts);
    printf("Write log per second %lu MB\n", (cnt * fflog.size() / 2^10) /(end_ts - start_ts));
}

int main(int argc, char** argv) {
    ofs.open("./PipeLog.log", std::ofstream::app); 
    //printf("Write %ld records, per record %d KB.\n", cnt, fflog.size());
    //OneThreadSyn(cnt);
    //OneThreadAsyn(cnt);
    printf("Write %ld records per thread, each record %d KB, thread num: %d.\n", mcnt, fflog.size(), N);
    MulThreadSyn();
    MulThreadAsyn();
    ofs.close();
    return 0;
}