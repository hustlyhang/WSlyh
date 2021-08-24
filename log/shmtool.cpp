#if 1
// 在程序退出后，将内存中的日志读出来
#include <limits.h>     // realpath
#include <stdlib.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "log.h"

#define SHMKEY 500234

int main(int argc, char** argv) {
    if (argc != 3 && argc != 4) {
        printf("%d\n", argc);
        printf("./flusher -f exefilepath [-d]\n");
        return -1;
    }
    char* exePath = nullptr;
    bool IsDel = false;
    int i = 1;
    while (i < argc) {
        if (strcmp(argv[i], "-f") == 0) {
            exePath = realpath(argv[i + 1], nullptr);
            i += 2;
        }
        else if (strcmp(argv[i], "-d") == 0) {
            IsDel = true;
            i += 1;
        }
        else {
            printf("./flusher -f exefilepath [-d]\n");
            return -1;
        }
    }
    if (!exePath) {
        printf("./flusher -f exefilepath [-d]\n");
        return -1;
    }


    key_t shmKey = ftok(exePath, SHMKEY);
    if (shmKey < 0)
    {
        perror("ftok");
        return -1;
    }
    int shmid = shmget(shmKey, sizeof(int*), 0666);
    if (shmid == -1)
    {
        perror("shmget");
        return -1;
    }
    //clear old cell_buffer shm here
    int* CBShmId = (int*)shmat(shmid, 0, 0);
    int headCBShmid = *CBShmId;
    int currCBShmid = headCBShmid;
    int nextCBShmid;
    bool have_data = false;
    FILE* fp = NULL;
    do
    {
        CBufferCell* cf = (CBufferCell*)shmat(currCBShmid, 0, 0);
        cf->m_aData = (char*)cf + sizeof(CBufferCell);
        if (!cf->IsEmpty())
        {
            have_data = true;
            if (!fp) fp = fopen("leaveLog.log", "w");
            cf->Persist(fp);
        }
        nextCBShmid = cf->m_iNextShmId;
        if (IsDel)
        {
            shmdt((void*)cf);
            shmctl(currCBShmid, IPC_RMID, NULL);
        }
        currCBShmid = nextCBShmid;
    } while (headCBShmid != currCBShmid);
    if (fp)
        fclose(fp);
    if (IsDel)
    {
        shmdt((void*)CBShmId);
        shmctl(shmid, IPC_RMID, NULL);
    }
    if (have_data)
    {
        printf("Left data persist to leaveLog.log\n");
    }
    else
    {
        printf("No extra data left in memory\n");
    }
    return 0;
}

#endif // 0