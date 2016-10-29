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

#include "dabInputFifo.h"
#include "dabInputPacketFile.h"
#include "dabInput.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>


#ifdef HAVE_FORMAT_PACKET
#   ifdef HAVE_INPUT_FIFO


int dabInputFifoData::nb = 0;


struct dabInputOperations dabInputFifoOperations = {
    dabInputFifoInit,
    dabInputFifoOpen,
    dabInputFifoSetbuf,
    dabInputFifoRead,
    dabInputFifoLock,
    dabInputFifoUnlock,
    dabInputPacketFileRead,
    dabInputSetbitrate,
    dabInputFifoClose,
    dabInputFifoClean,
    dabInputFifoRewind
};


int dabInputFifoInit(void** args)
{
    dabInputFifoData* data = new dabInputFifoData;
    memset(data, 0, sizeof(*data));
    data->stats.id = dabInputFifoData::nb++;
    data->maxSize = 0;
    data->curSize = 0;
    data->head = 0;
    data->tail = 0;
    data->buffer = nullptr;
    data->packetData = nullptr;
    data->enhancedPacketData = nullptr;
    data->packetLength = 0;
    data->enhancedPacketLength = 0;
    data->enhancedPacketWaiting = 0;
    data->full = false;
    data->running = true;
    data->thread = (pthread_t)NULL;
#ifdef _WIN32
    char semName[32];
    sprintf(semName, "semInfo%i", data->stats.id);
    data->semInfo = CreateSemaphore(NULL, 1, 1, semName);
    if (data->semInfo == NULL) {
        fprintf(stderr, "Can't init FIFO data semaphore %s\n", semName);
        return -1;
    }
    sprintf(semName, "semBuffer%i", data->stats.id);
    data->semBuffer = CreateSemaphore(NULL, 1, 1, semName);
    if (data->semBuffer == NULL) {
        fprintf(stderr, "Can't init FIFO buffer semaphore %s\n", semName);
        return -1;
    }
    sprintf(semName, "semFull%i", data->stats.id);
    data->semFull = CreateSemaphore(NULL, 1, 1, semName);
    if (data->semFull == NULL) {
        fprintf(stderr, "Can't init FIFO semaphore %s\n", semName);
        return -1;
    }
#else
    if (sem_init(&data->semInfo, 0, 1) == -1) {
        perror("Can't init FIFO data semaphore");
        return -1;
    }
    if (sem_init(&data->semBuffer, 0, 0) == -1) {
        perror("Can't init fIFO buffer semaphore");
        return -1;
    }
    if (sem_init(&data->semFull, 0, 0) == -1) {
        perror("Can't init FIFO semaphore");
        return -1;
    }
#endif

    if (data->maxSize > 0) {
#ifdef _WIN32
        ReleaseSemaphore(data->semBuffer, 1, NULL);
#else
        sem_post(&data->semBuffer);
#endif
    }
    *args = data;
    return 0;
}


int dabInputFifoOpen(void* args, const char* filename)
{
    dabInputFifoData* data = (dabInputFifoData*)args;
    data->file = open(filename, O_RDONLY | O_BINARY | O_NONBLOCK);
    if (data->file == -1) {
        perror(filename);
        return -1;
    }
#ifdef _WIN32
#else
    int flags = fcntl(data->file, F_GETFL);
    if (flags == -1) {
        perror(filename);
        return -1;
    }
    if (fcntl(data->file, F_SETFL, flags & ~O_NONBLOCK) == -1) {
        perror(filename);
        return -1;
    }
#endif

#ifdef _WIN32
    data->thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)dabInputFifoThread, data, 0, NULL);
    if (data->thread == NULL) {
        fprintf(stderr, "Can't create FIFO child\n");
        return -1;
    }
#else
    if (pthread_create(&data->thread, nullptr, dabInputFifoThread, data)) {
        perror("Can't create FIFO child");
        return -1;
    }
#endif

    return 0;
}


int dabInputFifoSetbuf(void* args, int size)
{
    dabInputFifoData* data = (dabInputFifoData*)args;

    if (data->maxSize > 0) {
#ifdef _WIN32
        WaitForSingleObject(data->semBuffer, INFINITE);
#else
        sem_wait(&data->semBuffer);
#endif
    }
    if (data->buffer != nullptr) {
        delete data->buffer;
    }
    if (size == 0) {
        size = 1024;
    }
    data->buffer = new unsigned char[size * 16];
    data->maxSize = size * 16;
#ifdef _WIN32
    ReleaseSemaphore(data->semBuffer, 1, NULL);
#else
    sem_post(&data->semBuffer);
#endif

    return 0;
}


int dabInputFifoRead(void* args, void* buffer, int size)
{
    //fprintf(stderr, "INFO: read %i bytes\n", size);
    dabInputFifoData* data = (dabInputFifoData*)args;
    dabInputFifoStats* stats = &data->stats;
    int head;
    int tail;
    int curSize;
    int maxSize;
#ifdef _WIN32
    WaitForSingleObject(data->semInfo, INFINITE);
#else
    sem_wait(&data->semInfo);
#endif
    head = data->head;
    tail = data->tail;
    curSize = data->curSize;
    maxSize = data->maxSize;
#ifdef _WIN32
    ReleaseSemaphore(data->semInfo, 1, NULL);
#else
    sem_post(&data->semInfo);
#endif
    //fprintf(stderr, "head: %i, tail: %i, curSize: %i\n", head, tail, curSize);
    if (size > curSize) {
        if (curSize == 0) {
            stats->empty = true;
        } else {
            etiLog.log(warn, "Not enough data in FIFO buffer: (%i) %i/%i\n",
                    data->stats.id, curSize, size);
        }
        return 0;
    }
    if (head > tail) {
        memcpy(buffer, data->buffer + tail, size);
#ifdef _WIN32
        WaitForSingleObject(data->semInfo, INFINITE);
#else
        sem_wait(&data->semInfo);
#endif
        data->tail += size;
        data->curSize -= size;
#ifdef _WIN32
        ReleaseSemaphore(data->semInfo, 1, NULL);
#else
        sem_post(&data->semInfo);
#endif
        return size;
    } else {
        if (maxSize - tail >= size) {
            memcpy(buffer, data->buffer + tail, size);
#ifdef _WIN32
            WaitForSingleObject(data->semInfo, INFINITE);
#else
            sem_wait(&data->semInfo);
#endif
            data->tail += size;
            data->curSize -= size;
#ifdef _WIN32
            ReleaseSemaphore(data->semInfo, 1, NULL);
#else
            sem_post(&data->semInfo);
#endif
            return size;
        } else {
            memcpy(buffer, data->buffer + tail, maxSize - tail);
#ifdef _WIN32
            WaitForSingleObject(data->semInfo, INFINITE);
#else
            sem_wait(&data->semInfo);
#endif
            data->tail = 0;
            data->curSize -= maxSize - tail;
#ifdef _WIN32
            ReleaseSemaphore(data->semInfo, 1, NULL);
#else
            sem_post(&data->semInfo);
#endif
            return maxSize - tail + dabInputFifoRead(data, (char*)buffer + maxSize - tail, size - (maxSize - tail));
        }
    }
    return -1;
}


int dabInputFifoLock(void* args) {
    dabInputFifoData* data = (dabInputFifoData*)args;
    dabInputFifoStats* stats = &data->stats;

    int maxSize;
    int curSize;
#ifdef _WIN32
    WaitForSingleObject(data->semInfo, INFINITE);
#else
    sem_wait(&data->semInfo);
#endif
    maxSize = data->maxSize;
    curSize = data->curSize;
#ifdef _WIN32
    ReleaseSemaphore(data->semInfo, 1, NULL);
#else
    sem_post(&data->semInfo);
#endif

    stats->bufferRecords[stats->bufferCount].curSize = curSize;
    stats->bufferRecords[stats->bufferCount].maxSize = maxSize;

    if (++stats->bufferCount == NB_RECORDS) {
        etiLog.log(info, "FIFO buffer state: (%i)", stats->id);
        for (int i = 0; i < stats->bufferCount; ++i) {
            etiLog.log(info, " %i/%i",
                    stats->bufferRecords[i].curSize,
                    stats->bufferRecords[i].maxSize);
        }
        etiLog.log(info, "\n");

        if (stats->full) {
            etiLog.log(warn, "FIFO buffer full: (%i)\n",
                    data->stats.id);
            stats->full = false;
        }
        if (stats->empty) {
            etiLog.log(warn, "FIFO buffer empty: (%i)\n",
                    data->stats.id);
            stats->empty = false;
        }
        if (stats->error) {
            etiLog.log(error, "FIFO input read error: (%i)\n",
                    data->stats.id);
            stats->error = false;
        }
        if (stats->input) {
            etiLog.log(error, "FIFO input not connected: (%i)\n",
                    data->stats.id);
            stats->input = false;
        }

        stats->bufferCount = 0;
    }
    return 0;
}


int dabInputFifoUnlock(void* args) {
    dabInputFifoData* data = (dabInputFifoData*)args;
    if (data->full) {
#ifdef _WIN32
        ReleaseSemaphore(data->semFull, 1, NULL);
#else
        sem_post(&data->semFull);
#endif
    }
    return 0;
}


int dabInputFifoClose(void* args)
{
    dabInputFifoData* data = (dabInputFifoData*)args;
    close(data->file);
    return 0;
}


int dabInputFifoClean(void** args)
{
    dabInputFifoData* data = (dabInputFifoData*)*args;
    data->running = false;
    etiLog.log(debug, "Wait FIFO child...\n");
#ifdef WIN32
    DWORD status;
    for (int i = 0; i < 5; ++i) {
        if (GetExitCodeThread(data->thread, &status)) {
            break;
        }
        Sleep(100);
    }
    TerminateThread(data->thread, 1);
    if (CloseHandle(data->thread) == 0) {
        etiLog.log(debug, "ERROR: Failed to close FIFO child thread\n");
    }
#else
    if (data->thread != (pthread_t)NULL) {
        if (pthread_join(data->thread, nullptr)) {
            etiLog.log(debug, "ERROR: FIFO child thread had not exit normally\n");
        }
    }
#endif
    etiLog.log(debug, "Done\n");
#ifdef _WIN32
    CloseHandle(data->semInfo);
    CloseHandle(data->semFull);
    CloseHandle(data->semBuffer);
#else
    sem_destroy(&data->semInfo);
    sem_destroy(&data->semFull);
    sem_destroy(&data->semBuffer);
#endif
    if (data->packetData != nullptr) {
        delete[] data->packetData;
    }
    if (data->enhancedPacketData != nullptr) {
        for (int i = 0; i < 12; ++i) {
            if (data->enhancedPacketData[i] != nullptr) {
                delete[] data->enhancedPacketData[i];
            }
        }
        delete[] data->enhancedPacketData;
    }
    delete data->buffer;
    delete data;
    return 0;
}


int dabInputFifoRewind(void* args)
{
    return -1;
}


void* dabInputFifoThread(void* args)
{
    dabInputFifoData* data = (dabInputFifoData*)args;
    int head;
    int tail;
    int curSize;
    int maxSize;
    int ret;
    while (data->running) {
#ifdef _WIN32
        WaitForSingleObject(data->semBuffer, INFINITE);
        WaitForSingleObject(data->semInfo, INFINITE);
#else
        sem_wait(&data->semBuffer);
        sem_wait(&data->semInfo);
#endif
        head = data->head;
        tail = data->tail;
        curSize = data->curSize;
        maxSize = data->maxSize;
#ifdef _WIN32
        ReleaseSemaphore(data->semInfo, 1, NULL);
#else
        sem_post(&data->semInfo);
#endif
        //fprintf(stderr, "thread, head: %i, tail: %i, curSize: %i\n", head, tail, curSize);

        if (curSize == maxSize) {
            data->stats.full = true;
            data->full = true;
#ifdef _WIN32
            WaitForSingleObject(data->semFull, INFINITE);
#else
            sem_wait(&data->semFull);
#endif
        } else if (head >= tail) {          // 2 blocks
            ret = read(data->file, data->buffer + head, maxSize - head);
            if (ret == 0) {
                data->stats.input = true;
                data->full = true;
#ifdef _WIN32
                WaitForSingleObject(data->semFull, INFINITE);
#else
                sem_wait(&data->semFull);
#endif
            } else if (ret == -1) {
                data->stats.error = true;
            } else {
#ifdef _WIN32
                WaitForSingleObject(data->semInfo, INFINITE);
#else
                sem_wait(&data->semInfo);
#endif
                data->head += ret;
                data->curSize += ret;
                if (data->head == maxSize) {
                    data->head = 0;
                }
#ifdef _WIN32
                ReleaseSemaphore(data->semInfo, 1, NULL);
#else
                sem_post(&data->semInfo);
#endif
            }
        } else {   // 1 block
            ret = read(data->file, data->buffer + head, tail - head);
            if (ret == 0) {
                data->stats.input = true;
                data->full = true;
#ifdef _WIN32
                WaitForSingleObject(data->semFull, INFINITE);
#else
                sem_wait(&data->semFull);
#endif
            } else if (ret == -1) {
                data->stats.error = true;
            } else {
#ifdef _WIN32
                WaitForSingleObject(data->semInfo, INFINITE);
#else
                sem_wait(&data->semInfo);
#endif
                data->head += ret;
                data->curSize += ret;
                if (data->head == maxSize) {
                    data->head = 0;
                }
#ifdef _WIN32
                ReleaseSemaphore(data->semInfo, 1, NULL);
#else
                sem_post(&data->semInfo);
#endif
            }
        }
#ifdef _WIN32
        ReleaseSemaphore(data->semBuffer, 1, NULL);
#else
        sem_post(&data->semBuffer);
#endif
    }
    return nullptr;
}


#   endif
#endif
