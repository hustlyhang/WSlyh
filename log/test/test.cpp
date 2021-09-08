// test syn

#include "../log.h"
#include <unistd.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>
#include <fstream>

const long long cnt = 1e7;
std::string fflog("test test test test test test test test test test test test test test test test test test test test");


int64_t get_current_millis(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

void OneThreadAsyn(long long cnt) {
    LOG_INIT("./PipeLog.log", "test", 3);
    uint64_t start_ts = get_current_millis();
    for (int i = 0;i < cnt; ++i) {
        LOG_ERROR("test test test test test test test test test test test test test test test test test test test test");
    }
    uint64_t end_ts = get_current_millis();
    printf("Asyn time use %lums\n", end_ts - start_ts);
}
void OneThreadSyn(std::string& log, long long cnt) {
    std::ofstream ofs;
    ofs.open("./PipeLog.log", std::ofstream::app); 
    uint64_t start_ts = get_current_millis();
    for (int i = 0; i < cnt; ++i) {
        time_t t = time(0);
        char tmp[64];
        strftime(tmp, sizeof(tmp), "[%Y-%m-%d %X]", localtime(&t));  
        ofs << tmp << " - ";
        ofs.write(log.c_str(), log.size());  
        ofs << std::endl;
    }
    ofs.close();
    uint64_t end_ts = get_current_millis();
    printf("Syn time use %lums\n", end_ts - start_ts);
}

int main(int argc, char** argv) {
    OneThreadSyn(fflog, cnt);
    OneThreadAsyn(cnt);
    return 0;
}