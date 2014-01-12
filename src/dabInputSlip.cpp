/*
   Copyright (C) 2009 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)
   */
/*
   This file is part of CRC-DabMux.

   CRC-DabMux is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   CRC-DabMux is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with CRC-DabMux.  If not, see <http://www.gnu.org/licenses/>.
   */

#include "dabInputSlip.h"
#include "dabInputFifo.h"
#include "TcpServer.h"
#include "UdpSocket.h"
#include "bridge.h"

#include <signal.h>
#include <string.h>
#include <limits.h>


#ifdef _WIN32
#   include <io.h>

#   define sem_t HANDLE
#   define O_NONBLOCK 0
#else
#   include <semaphore.h>
#   define O_BINARY 0
#endif


struct dabInputSlipData {
    TcpServer* server;
    UdpPacket** packets;
    UdpPacket* buffer;
    bridgeInfo* info;
    dabInputFifoStats stats;
    pthread_t thread;
    sem_t semWrite;
    sem_t semQueue;
    bool reading;
    volatile int nbPackets;
    volatile int queueSize;
    volatile int packetSize;
};


struct dabInputOperations dabInputSlipOperations = {
    dabInputSlipInit,
    dabInputSlipOpen,
    dabInputSlipSetbuf,
    dabInputSlipRead,
    NULL,
    NULL,
    dabInputSlipReadFrame,
    dabInputSetbitrate,
    dabInputSlipClose,
    dabInputSlipClean,
    NULL
};


int dabInputSlipInit(void** args)
{
    dabInputSlipData* data = new dabInputSlipData;
    memset(&data->stats, 0, sizeof(data->stats));
    data->stats.id = dabInputFifoData::nb++;
    data->server = new TcpServer();
    data->packetSize = 1500;
    data->queueSize = 10;
    data->packets = new UdpPacket*[data->queueSize];
    for (int i = 0; i < data->queueSize; ++i) {
        data->packets[i] = new UdpPacket(data->packetSize);
    }
    data->buffer = new UdpPacket(data->packetSize);
    data->nbPackets = 0;
    data->info = new bridgeInfo;
    data->thread = (pthread_t)NULL;
    bridgeInitInfo(data->info);

#ifdef _WIN32
    char semName[32];
    sprintf(semName, "semWrite%i", data->stats.id);
    data->semWrite = CreateSemaphore(NULL, 1, 1, semName);
    if (data->semWrite == NULL) {
        fprintf(stderr, "Can't init SLIP data write semaphore %s\n", semName);
        return -1;
    }
    sprintf(semName, "semQueue%i", data->stats.id);
    data->semQueue = CreateSemaphore(NULL, 1, 1, semName);
    if (data->semQueue == NULL) {
        fprintf(stderr, "Can't init SLIP data index semaphore %s\n", semName);
        return -1;
    }
#else
    if (sem_init(&data->semWrite, 0, data->queueSize) == -1) {
        perror("Can't init SLIP data write semaphore");
        return -1;
    }
    if (sem_init(&data->semQueue, 0, 1) == -1) {
        perror("Can't init SLIP data index semaphore");
        return -1;
    }
#endif
    data->reading = false;

    *args = data;
    return 0;
}


void* dabInputSlipThread(void* args)
{
    dabInputSlipData* data = (dabInputSlipData*)args;
    TcpSocket* client;

    while ((client = data->server->accept()) != NULL) {
        int size = 0;
        etiLog.log(info, "SLIP server got a new client.\n");

#ifdef _WIN32
        WaitForSingleObject(data->semWrite, INFINITE);
                WaitForSingleObject(data->semQueue, INFINITE);
#else
        sem_wait(&data->semWrite);
        sem_wait(&data->semQueue);
#endif
        UdpPacket* packet = data->packets[data->nbPackets];
#ifdef _WIN32
        ReleaseSemaphore(data->semQueue, 1, NULL);
#else
        sem_post(&data->semQueue);
#endif

        while ((size = client->read(packet->getData(), packet->getSize()))
                    > 0) {
            packet->setLength(size);
#ifdef _WIN32
            WaitForSingleObject(data->semQueue, INFINITE);
#else
            sem_wait(&data->semQueue);
#endif
            data->nbPackets++;
#ifdef _WIN32
            ReleaseSemaphore(data->semQueue, 1, NULL);
#else
            sem_post(&data->semQueue);
#endif
            
#ifdef _WIN32
            WaitForSingleObject(data->semWrite, INFINITE);
            WaitForSingleObject(data->semQueue, INFINITE);
#else
            sem_wait(&data->semWrite);
            sem_wait(&data->semQueue);
#endif
            packet = data->packets[data->nbPackets];
#ifdef _WIN32
            ReleaseSemaphore(data->semQueue, 1, NULL);
#else
            sem_post(&data->semQueue);
#endif
        }
        etiLog.log(info, "SLIP server client deconnected.\n");
        client->close();
    }
    etiLog.log(error, "SLIP thread can't accept new client (%s)\n",
            inetErrDesc, inetErrMsg);

    return NULL;
}


int dabInputSlipOpen(void* args, const char* inputName)
{
    const char* address;
    long port;
    address = strchr(inputName, ':');
    if (address == NULL) {
        etiLog.log(error, "\"%s\" SLIP address format is invalid: "
                "should be [address]:port - > aborting\n", inputName);
        return -1;
    }
    ++address;
    port = strtol(address, (char **)NULL, 10);
    if ((port == LONG_MIN) || (port == LONG_MAX)) {
        etiLog.log(error, "can't convert port number in SLIP address %s\n",
                address);
        return -1;
    }
    if (port == 0) {
        etiLog.log(error, "can't use port number 0 in SLIP address\n");
        return -1;
    }
    dabInputSlipData* data = (dabInputSlipData*)args;
    if (data->server->create(port) == -1) {
        etiLog.log(error, "can't set port %i on SLIP input (%s: %s)\n",
                port, inetErrDesc, inetErrMsg);
        return -1;
    }

    if (data->server->listen() == -1) {
        etiLog.log(error, "can't listen on SLIP socket(%s: %s)\n",
                inetErrDesc, inetErrMsg);
        return -1;
    }
#ifdef _WIN32
    data->thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)dabInputSlipThread, data, 0, NULL);
    if (data->thread == NULL) {
        fprintf(stderr, "Can't create SLIP child");
        return -1;
    }
#else
    if (pthread_create(&data->thread, NULL, dabInputSlipThread, data)) {
        perror("Can't create SLIP child");
        return -1;
    }
#endif

    etiLog.log(debug, "check return code of create\n");
    return 0;
}


int dabInputSlipSetbuf(void* args, int size)
{
    dabInputSlipData* data = (dabInputSlipData*)args;

    if (size <= 10) {
        return -1;
    }

    data->packetSize = size - 10;
    if (data->packets == NULL) {
        data->packets = new UdpPacket*[data->queueSize];
    }
    for (int i = 0; i < data->queueSize; ++i) {
        if (data->packets[i] == NULL) {
            data->packets[i] = new UdpPacket(data->packetSize);
        } else {
            data->packets[i]->setSize(data->packetSize);
        }
    }
    if (data->buffer == NULL) {
        data->buffer = new UdpPacket(data->packetSize);
    } else {
        data->buffer->setSize(data->packetSize);
    }

    return 0;
}


int dabInputSlipRead(void* args, void* buffer, int size)
{
    dabInputSlipData* data = (dabInputSlipData*)args;

    if (data->nbPackets > 0) {  // data ready
        UdpPacket* temp;
        temp = data->buffer;
        data->buffer = data->packets[0];

#ifdef _WIN32
        WaitForSingleObject(data->semQueue, INFINITE);
#else
        sem_wait(&data->semQueue);
#endif
        for (int i = 1; i < data->queueSize; ++i) {
            data->packets[i - 1] = data->packets[i];
        }
        data->packets[data->queueSize - 1] = temp;
        --data->nbPackets;
#ifdef _WIN32
        ReleaseSemaphore(data->semQueue, 1, NULL);
        ReleaseSemaphore(data->semWrite, 1, NULL);
#else
        sem_post(&data->semQueue);
        sem_post(&data->semWrite);
#endif
    } else {
        data->buffer->setLength(0);
    }

    return data->buffer->getLength();
}


int dabInputSlipReadFrame(dabInputOperations* ops, void* args, void* buffer, int size)
{
    int nbBytes = 0;
    dabInputSlipData* data = (dabInputSlipData*)args;
    dabInputFifoStats* stats = (dabInputFifoStats*)&data->stats;

#ifdef _WIN32
    WaitForSingleObject(data->semQueue, INFINITE);
#else
    sem_wait(&data->semQueue);
#endif
    stats->bufferRecords[stats->bufferCount].curSize = data->nbPackets;
    stats->bufferRecords[stats->bufferCount].maxSize = data->queueSize;
#ifdef _WIN32
    ReleaseSemaphore(data->semQueue, 1, NULL);
#else
    sem_post(&data->semQueue);
#endif
    if (++stats->bufferCount == NB_RECORDS) {
        etiLog.log(info, "SLIP buffer state: (%i)", stats->id);
        for (int i = 0; i < stats->bufferCount; ++i) {
            etiLog.log(info, " %i/%i",
                    stats->bufferRecords[i].curSize,
                    stats->bufferRecords[i].maxSize);
        }
        etiLog.log(info, "\n");

        stats->bufferCount = 0;
    }

    data->stats.frameRecords[data->stats.frameCount].curSize = 0;
    data->stats.frameRecords[data->stats.frameCount].maxSize = size;

    if (data->buffer->getLength() == 0) {
        ops->read(args, NULL, 0);
    }
    while ((nbBytes = writePacket(data->buffer->getData(),
                    data->buffer->getLength(), buffer, size, data->info))
            != 0) {
        data->stats.frameRecords[data->stats.frameCount].curSize = nbBytes;
        ops->read(args, NULL, 0);
    }

    if (data->buffer->getLength() != 0) {
        data->stats.frameRecords[data->stats.frameCount].curSize = size;
    }

    if (++stats->frameCount == NB_RECORDS) {
        etiLog.log(info, "Data subchannel usage: (%i)",
                stats->id);
        for (int i = 0; i < stats->frameCount; ++i) {
            etiLog.log(info, " %i/%i",
                    stats->frameRecords[i].curSize,
                    stats->frameRecords[i].maxSize);
        }
        etiLog.log(info, "\n");
        stats->frameCount = 0;
    }
    return size;
}


int dabInputSlipClose(void* args)
{
    dabInputSlipData* data = (dabInputSlipData*)args;
    data->server->close();
#ifdef WIN32
    DWORD status;
    for (int i = 0; i < 5; ++i) {
        if (GetExitCodeThread(data->thread, &status)) {
            break;
        }
        Sleep(100);
    }
    TerminateThread(data->thread, 1);
#else
    if (data->thread != (pthread_t)NULL) {
        pthread_kill(data->thread, SIGPIPE);
    }
#endif
    return 0;
}


int dabInputSlipClean(void** args)
{
    dabInputSlipData* data = (dabInputSlipData*)(*args);
#ifdef _WIN32
    CloseHandle(data->thread);
    CloseHandle(data->semWrite);
    CloseHandle(data->semQueue);
#else
    sem_destroy(&data->semWrite);
    sem_destroy(&data->semQueue);
#endif
    for (int i = 0; i < data->queueSize; ++i) {
        if (data->packets[i] != NULL) {
            delete data->packets[i];
        }
    }
    delete []data->packets;
    delete data->buffer;
    delete data->server;
    delete data->info;
    delete data;
    return 0;
}


