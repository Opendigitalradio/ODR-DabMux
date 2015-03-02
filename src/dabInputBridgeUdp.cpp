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

#include "dabInputBridgeUdp.h"
#include "dabInputUdp.h"
#include "bridge.h"

#ifdef HAVE_FORMAT_BRIDGE
#   ifdef HAVE_INPUT_UDP

struct dabInputBridgeUdpData {
    dabInputUdpData* udpData;
    bridgeInfo* info;
};


struct dabInputOperations dabInputBridgeUdpOperations = {
    dabInputBridgeUdpInit,
    dabInputBridgeUdpOpen,
    dabInputSetbuf,
    NULL,
    NULL,
    NULL,
    dabInputBridgeUdpRead,
    dabInputSetbitrate,
    dabInputBridgeUdpClose,
    dabInputBridgeUdpClean,
    NULL
};


int dabInputBridgeUdpInit(void** args)
{
    dabInputBridgeUdpData* input = new dabInputBridgeUdpData;
    dabInputUdpInit((void**)&input->udpData);
    input->info = new bridgeInfo;
    bridgeInitInfo(input->info);
    *args = input;

    return 0;
}


int dabInputBridgeUdpOpen(void* args, const char* inputName)
{
    dabInputBridgeUdpData* input = (dabInputBridgeUdpData*)args;

    return dabInputUdpOpen(input->udpData, inputName);
}


int dabInputBridgeUdpRead(dabInputOperations* ops, void* args, void* buffer, int size)
{
    int nbBytes = 0;
    dabInputBridgeUdpData* input = (dabInputBridgeUdpData*)args;
    dabInputFifoStats* stats = (dabInputFifoStats*)&input->udpData->stats;

    stats->frameRecords[stats->frameCount].curSize = 0;
    stats->frameRecords[stats->frameCount].maxSize = size;

    if (input->udpData->packet->getLength() == 0) {
        input->udpData->socket->receive(*input->udpData->packet);
    }
    while ((nbBytes = writePacket(input->udpData->packet->getData(),
                    input->udpData->packet->getLength(), buffer, size,
                    input->info))
            != 0) {
        stats->frameRecords[stats->frameCount].curSize = nbBytes;
        input->udpData->socket->receive(*input->udpData->packet);
    }

    if (input->udpData->packet->getLength() != 0) {
        stats->frameRecords[stats->frameCount].curSize = size;
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


int dabInputBridgeUdpClose(void* args)
{
    dabInputBridgeUdpData* input = (dabInputBridgeUdpData*)args;

    return dabInputUdpClose(input->udpData);
}


int dabInputBridgeUdpClean(void** args)
{
    dabInputBridgeUdpData* input = (dabInputBridgeUdpData*)(*args);
    dabInputUdpClean((void**)&input->udpData);
    delete input->info;
    delete input;
    return 0;
}


#   endif // HAVE_INPUT_UDP
#endif // HAVE_FORMAT_BRIDGE

