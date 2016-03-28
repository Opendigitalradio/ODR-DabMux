/*
   Copyright (C) 2009 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)
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

#include "dabInputUdp.h"

#include <string.h>
#include <limits.h>


#ifdef __MINGW32__
#   define bzero(s, n) memset(s, 0, n)
#endif

#ifdef HAVE_FORMAT_RAW
#   ifdef HAVE_INPUT_UDP


struct dabInputOperations dabInputUdpOperations = {
    dabInputUdpInit,
    dabInputUdpOpen,
    dabInputSetbuf,
    NULL,
    NULL,
    NULL,
    dabInputUdpRead,
    dabInputSetbitrate,
    dabInputUdpClose,
    dabInputUdpClean,
    NULL
};


int dabInputUdpInit(void** args)
{
    dabInputUdpData* input = new dabInputUdpData;
    memset(&input->stats, 0, sizeof(input->stats));
    input->stats.id = dabInputFifoData::nb++;
    input->socket = new UdpSocket();
    input->packet = new UdpPacket(2048);
    *args = input;

    UdpSocket::init();
    return 0;
}


int dabInputUdpOpen(void* args, const char* inputName)
{
    int returnCode = 0;
    char* address;
    char* ptr;
    long port;
    dabInputUdpData* input = (dabInputUdpData*)args;

    // Skip the udp:// part if it is present
    if (strncmp(inputName, "udp://", 6) == 0) {
        address = strdup(inputName + 6);
    }
    else {
        address = strdup(inputName);
    }

    ptr = strchr(address, ':');
    if (ptr == NULL) {
        etiLog.log(error,
                "\"%s\" is an invalid format for udp address: "
                "should be [udp://][address]:port - > aborting\n", address);
        returnCode = -1;
        goto udpopen_ptr_null_out;
    }
    *(ptr++) = 0;
    port = strtol(ptr, (char **)NULL, 10);
    if ((port == LONG_MIN) || (port == LONG_MAX)) {
        etiLog.log(error,
                "can't convert port number in udp address %s\n",
                address);
        returnCode = -1;
    }
    if (port == 0) {
        etiLog.log(error, "can't use port number 0 in udp address\n");
        returnCode = -1;
    }
    if (input->socket->create(port) == -1) {
        etiLog.log(error, "can't set port %i on Udp input (%s: %s)\n",
                port, inetErrDesc, inetErrMsg);
        returnCode = -1;
    }

    if (*address != 0) {
        if (input->socket->joinGroup(address) == -1) {
            etiLog.log(error,
                    "can't join multicast group %s (%s: %s)\n",
                    address, inetErrDesc, inetErrMsg);
            returnCode = -1;
        }
    }

    if (input->socket->setBlocking(false) == -1) {
        etiLog.log(error, "can't set Udp input socket in blocking mode "
                "(%s: %s)\n", inetErrDesc, inetErrMsg);
        returnCode = -1;
    }

udpopen_ptr_null_out:
    free(address);
    etiLog.log(debug, "check return code of create\n");
    return returnCode;
}


int dabInputUdpRead(dabInputOperations* ops, void* args, void* buffer, int size)
{
    int nbBytes = 0;
    uint8_t* data = reinterpret_cast<uint8_t*>(buffer);

    dabInputUdpData* input = (dabInputUdpData*)args;
    dabInputFifoStats* stats = (dabInputFifoStats*)&input->stats;

    input->stats.frameRecords[input->stats.frameCount].curSize = 0;
    input->stats.frameRecords[input->stats.frameCount].maxSize = size;

    if (input->packet->getLength() == 0) {
        input->socket->receive(*input->packet);
    }

    while (nbBytes < size) {
        unsigned freeSize = size - nbBytes;
        if (input->packet->getLength() > freeSize) {
            // Not enought place in output
            memcpy(&data[nbBytes], input->packet->getData(), freeSize);
            nbBytes = size;
            input->packet->setOffset(input->packet->getOffset() + freeSize);
        } else {
            unsigned length = input->packet->getLength();
            memcpy(&data[nbBytes], input->packet->getData(), length);
            nbBytes += length;
            input->packet->setOffset(0);
            input->socket->receive(*input->packet);
            if (input->packet->getLength() == 0) {
                break;
            }
        } 
    }
    input->stats.frameRecords[input->stats.frameCount].curSize = nbBytes;
    bzero(&data[nbBytes], size - nbBytes);
    
    input->stats.frameRecords[input->stats.frameCount].curSize = nbBytes;
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


int dabInputUdpClose(void* args)
{
    return 0;
}


int dabInputUdpClean(void** args)
{
    dabInputUdpData* input = (dabInputUdpData*)(*args);
    delete input->socket;
    delete input->packet;
    delete input;
    return 0;
}


#   endif
#endif
