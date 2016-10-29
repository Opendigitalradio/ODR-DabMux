/*
   Copyright (C) 2009 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2016
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://www.opendigitalradio.org
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

#pragma once

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "dabInputFile.h"
#include "Log.h"


#ifdef _WIN32
#   include <io.h>
#   define sem_t HANDLE
#   define O_NONBLOCK 0
#else
#   include <semaphore.h>
#   define O_BINARY 0
#endif


#ifdef HAVE_FORMAT_PACKET
#   ifdef HAVE_INPUT_FIFO
extern struct dabInputOperations dabInputFifoOperations;


struct dabInputFifoStatRecord {
    int curSize;
    int maxSize;
};


#define NB_RECORDS      10
struct dabInputFifoStats {
    int id;
    bool full;
    bool empty;
    bool error;
    bool input;
    int bufferCount;
    dabInputFifoStatRecord bufferRecords[NB_RECORDS];
    int frameCount;
    dabInputFifoStatRecord frameRecords[NB_RECORDS];
};


struct dabInputFifoData : dabInputFileData {
    static int nb;
    int maxSize;
    int curSize;
    int head;
    int tail;
    dabInputFifoStats stats;
    unsigned char* buffer;
    pthread_t thread;
    sem_t semInfo;
    sem_t semBuffer;
    sem_t semFull;
    bool full;
    bool running;
};


int dabInputFifoInit(void** args);
int dabInputFifoOpen(void* args, const char* filename);
int dabInputFifoSetbuf(void* args, int size);
int dabInputFifoRead(void* args, void* buffer, int size);
int dabInputFifoLock(void* args);
int dabInputFifoUnlock(void* args);
int dabInputFifoClose(void* args);
int dabInputFifoClean(void** args);
int dabInputFifoRewind(void* args);
void* dabInputFifoThread(void* args);


#   endif
#endif

