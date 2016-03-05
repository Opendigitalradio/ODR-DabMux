/*
   Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Her Majesty the Queen in
   Right of Canada (Communications Research Center Canada)

   Copyright (C) 2013 Matthias P. Braendli
   http://mpb.li

   SIMUL throttling output. It guarantees correct frame generation rate
   */
/*
   This file is part of ODR-DabMux.

   ODR-DabMux is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   ODR-DabMux is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with ODR-DabMux.  If not, see <http://www.gnu.org/licenses/>.
   */
#include "dabOutput.h"
#include <cstring>
#include <cstdio>
#include <fcntl.h>
#include <limits.h>
#include <chrono>
#include <thread>
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
    startTime_ = std::chrono::steady_clock::now();
#endif

    return 0;
}

int DabOutputSimul::Write(void* buffer, int size)
{
#ifdef _WIN32
    unsigned long current;
    unsigned long start;
    unsigned long waiting;
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
    auto curTime = std::chrono::steady_clock::now();

    auto diff = curTime - startTime_;
    auto waiting = std::chrono::milliseconds(24) - diff;

    if (diff < std::chrono::milliseconds(24)) {
            std::this_thread::sleep_for(waiting);
    }

    startTime_ += std::chrono::milliseconds(24);
#endif

    return size;
}
