/*
   Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Her Majesty the Queen in
   Right of Canada (Communications Research Center Canada)

   Copyright (C) 2018
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://www.opendigitalradio.org

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


int DabOutputSimul::Open(const char* name)
{
    startTime_ = std::chrono::steady_clock::now();
    return 0;
}

int DabOutputSimul::Write(void* buffer, int size)
{
    auto curTime = std::chrono::steady_clock::now();

    const auto frameinterval = std::chrono::milliseconds(24);

    auto diff = curTime - startTime_;
    auto waiting = frameinterval - diff;

    if (diff < frameinterval) {
            std::this_thread::sleep_for(waiting);
    }

    startTime_ += frameinterval;

    return size;
}

