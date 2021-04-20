#include "util.h"
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>


/*
    ��ȡ���ݣ�����ȡ��ָ�����ݻ���Ϊ�ź�EAGAINʱ˵����ȡ���
*/
ssize_t readn(int fd, char* buf, size_t n){
    ssize_t ans = 0, leftover = n;

    char* pos = buf;
    while (leftover > 0) {
        ssize_t ret = read(fd, pos, leftover);
        //? �ж��Ƿ��ȡ�ɹ�
        if (ret < 0) {
            //? ��ȡʧ��
            //? �жϴ���
            if (errno == EINTR) {
                //? ϵͳ�ж�
                ret = 0;
            }
            else if (errno == EAGAIN) {
                //? ��ȡ���
                return ans;
            }
            else {
                //? ����
                return -1;
            }
        }
        ans += ret;
        pos += ret;
        leftover -= ret;
    }
    return ans;
}
/*
    д�����ݣ���Ϊ֪��Ҫд��������ݣ����Ե����ź�EAGAINʱ����Ҫ����д��
*/
ssize_t writen(int fd, char* buf, size_t n){
    ssize_t ans = 0, leftover = n;

    char* pos = buf;
    while (leftover > 0) {
        ssize_t ret = write(fd, pos, leftover);
        //? �ж��Ƿ�д��ɹ�
        if (ret < 0) {
            //? ��ȡʧ��
            //? �жϴ���
            if (errno == EINTR || errno == EAGAIN) {
                //? д���жϣ���Ҫ�ٴ�д��
                ret = 0;
                continue;
            }
            else {
                //? ����
                return -1;
            }
        }
        ans += ret;
        pos += ret;
        leftover -= ret;
    }
    return ans;
}

/*
    ����SIGPIPE�ź�,��ֹ�߳��˳�
    �ɹ�����0��ʧ�ܷ���-1��
*/
int hand_sigpipe(){
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_flags = 0;
    sa.sa_handler = SIG_IGN;
    int ret = sigaction(SIGPIPE, &sa, NULL);
    if (ret < 0) return -1;
    return 0;
}
/*
    ���ļ�����������Ϊ������
    �ɹ�����0��ʧ�ܷ���-1��
*/
int setSocketNonBlocking(int fd){
    int flg = fcntl(fd, F_GETFL, 0);
    if (flg < 0) return -1;
    flg |= O_NONBLOCK;
    int ret = fcntl(fd, flg, 0);
    if (ret < 0) return -1;
    return 0;
}
