#ifndef UTIL
#define UTIL
#include <cstdlib>
/*
    ÖØÐ´¶ÁÐ´º¯Êý
*/


ssize_t readn(int fd, char* buffer, size_t n);
ssize_t writen(int fd, char* buffer, size_t n);

int hand_sigpipe();
int setSocketNonBlocking(int fd);


#endif // !UTIL