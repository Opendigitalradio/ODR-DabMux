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

#include "dabInputDmbUdp.h"
#include "dabInputFifo.h"
#include "Dmb.h"
#include "UdpSocket.h"

#include <string.h>
#include <limits.h>


#ifdef HAVE_FORMAT_DMB
#   ifdef HAVE_INPUT_UDP


struct dabInputDmbUdpData {
    UdpSocket* socket;
    UdpPacket* packet;
    Dmb* dmb;
    dabInputFifoStats stats;
};


struct dabInputOperations dabInputDmbUdpOperations = {
    dabInputDmbUdpInit,
    dabInputDmbUdpOpen,
    dabInputSetbuf,
    NULL,
    NULL,
    NULL,
    dabInputDmbUdpRead,
    dabInputSetbitrate,
    dabInputDmbUdpClose,
    dabInputDmbUdpClean,
    NULL
};


int dabInputDmbUdpInit(void** args)
{
    dabInputDmbUdpData* input = new dabInputDmbUdpData;
    memset(&input->stats, 0, sizeof(input->stats));
    input->stats.id = dabInputFifoData::nb++;
    input->socket = new UdpSocket();
    input->packet = new UdpPacket(2048);
    input->dmb = new Dmb();
    *args = input;

    UdpSocket::init();
    return 0;
}


int dabInputDmbUdpOpen(void* args, const char* inputName)
{
    int returnCode = 0;
    char* address;
    char* ptr;
    long port;
    dabInputDmbUdpData* input = (dabInputDmbUdpData*)args;

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
                "should be [address]:port - > aborting\n", address);
        returnCode = -1;
        goto dmbudpopen_ptr_null_out;
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
        etiLog.log(error, "can't set port %i on Dmb input (%s: %s)\n",
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
        etiLog.log(error, "can't set Dmb input socket in blocking mode "
                "(%s: %s)\n", inetErrDesc, inetErrMsg);
        returnCode = -1;
    }

dmbudpopen_ptr_null_out:
    free(address);
    etiLog.log(debug, "check return code of create\n");
    return returnCode;;
}


int dabInputDmbUdpRead(dabInputOperations* ops, void* args, void* buffer, int size)
{
    int nbBytes = 0;
    dabInputDmbUdpData* input = (dabInputDmbUdpData*)args;
    dabInputFifoStats* stats = (dabInputFifoStats*)&input->stats;

    input->stats.frameRecords[input->stats.frameCount].curSize = 0;
    input->stats.frameRecords[input->stats.frameCount].maxSize = size;

    if (input->packet->getLength() == 0) {
        input->socket->receive(*input->packet);
    }
/*    while ((nbBytes = writePacket(input->packet->getData(),
                    input->packet->getLength(), buffer, size, input->info))
            != 0) {
        input->stats.frameRecords[input->stats.frameCount].curSize = nbBytes;
        input->socket->receive(*input->packet);
    }*/
    while ((nbBytes = input->dmb->encode(input->packet->getData(),
                    input->packet->getLength(), buffer, size))
            != 0) {
        input->stats.frameRecords[input->stats.frameCount].curSize = nbBytes;
        input->socket->receive(*input->packet);
        //++mpgFrameNb;
    }
    //++dmbFrameNb;

    if (input->packet->getLength() != 0) {
        input->stats.frameRecords[input->stats.frameCount].curSize = size;
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


int dabInputDmbUdpClose(void* args)
{
    return 0;
}


int dabInputDmbUdpClean(void** args)
{
    dabInputDmbUdpData* input = (dabInputDmbUdpData*)(*args);
    delete input->socket;
    delete input->packet;
    delete input->dmb;
    delete input;
    return 0;
}


#   endif
#endif
