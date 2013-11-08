#include "dabOutput.h"
#include <cstring>
#include <cstdio>
#include <fcntl.h>
#include <limits.h>
#ifdef _WIN32
#   include <io.h>
#   ifdef __MINGW32__
#       define FS_DECLARE_CFG_ARRAYS
#       include <winioctl.h>
#   endif
#   include <sdci.h>
#else
#   include <unistd.h>
#   include <sys/time.h>
#   ifndef O_BINARY
#       define O_BINARY 0
#   endif // O_BINARY
#endif


int DabOutputSimul::Open(const char* name)
{
#ifdef _WIN32
    startTime_ = GetTickCount();
#else
    gettimeofday(&startTime_, NULL);
#endif

    return 0;
}

int DabOutputSimul::Write(void* buffer, int size)
{
    unsigned long current;
    unsigned long start;
    unsigned long waiting;

#ifdef _WIN32
    current = GetTickCount();
    start = this->startTime_;
    if (current < start) {
        waiting = start - current + 24;
        Sleep(waiting);
    } else {
        waiting = 24 - (current - start);
        if ((current - start) < 24) {
            Sleep(waiting);
        }
    }
    this->startTime_ += 24;
#else
    timeval curTime;
    gettimeofday(&curTime, NULL);
    current = (1000000ul * curTime.tv_sec) + curTime.tv_usec;
    start = (1000000ul * this->startTime_.tv_sec) + this->startTime_.tv_usec;
    waiting = 24000ul - (current - start);
    if ((current - start) < 24000ul) {
        usleep(waiting);
    }

    this->startTime_.tv_usec += 24000;
    if (this->startTime_.tv_usec >= 1000000) {
        this->startTime_.tv_usec -= 1000000;
        ++this->startTime_.tv_sec;
    }
#endif

    return size;
}
