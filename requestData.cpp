#include "requestData.h"
#include "util.h"
#include "epoll.h"
#include <sys/epoll.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <unordered_map>
#include <fcntl.h>
#include <cstring>
#include <queue>

#include <iostream>

std::priority_queue<CTimer *, std::queue<CTimer *>, TimerCmp> q;
pthread_mutex_t qlock = PTHREAD_MUTEX_INITIALIZER;

// ��ʼ����̬����
pthread_mutex_t MyType::lock = PTHREAD_MUTEX_INITIALIZER;

std::unordered_map<std::string, std::string> MyType::mytype;

std::string MyType::GetType(const std::string &str)
{
    // DCL˫�������̰߳�ȫ, �ڴ�������������������
    if (mytype.size() == 0)
    {
        pthread_mutex_lock(&lock);
        if (mytype.size() == 0)
        {
            mytype[".html"] = "text/html";
            mytype[".avi"] = "video/x-msvideo";
            mytype[".bmp"] = "image/bmp";
            mytype[".c"] = "text/plain";
            mytype[".doc"] = "application/msword";
            mytype[".gif"] = "image/gif";
            mytype[".gz"] = "application/x-gzip";
            mytype[".htm"] = "text/html";
            mytype[".ico"] = "application/x-ico";
            mytype[".jpg"] = "image/jpeg";
            mytype[".png"] = "image/png";
            mytype[".txt"] = "text/plain";
            mytype[".mp3"] = "audio/mp3";
            mytype["default"] = "text/html";
        }
        pthread_mutex_unlock(&lock);
    }
    else
    {
        if (mytype.count(str) == 0)
            return mytype["default"];
        else
            return mytype[str];
    }
}
