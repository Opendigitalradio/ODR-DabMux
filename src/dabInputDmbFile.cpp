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

#include "dabInputDmbFile.h"
#include "dabInputFifo.h"
#include "Dmb.h"
#include "UdpSocket.h"

#include <string.h>
#include <stdio.h>

#ifdef HAVE_FORMAT_DMB

struct dabInputDmbFileData {
    FILE* file;
    Dmb* dmb;
    dabInputFifoStats stats;
    unsigned char buffer[188];
    unsigned bufferLength;
};


struct dabInputOperations dabInputDmbFileOperations = {
    dabInputDmbFileInit,
    dabInputDmbFileOpen,
    dabInputSetbuf,
    NULL,
    NULL,
    NULL,
    dabInputDmbFileRead,
    dabInputSetbitrate,
    dabInputDmbFileClose,
    dabInputDmbFileClean,
    NULL
};


int dabInputDmbFileInit(void** args)
{
    dabInputDmbFileData* input = new dabInputDmbFileData;
    memset(&input->stats, 0, sizeof(input->stats));
    input->stats.id = dabInputFifoData::nb++;
    input->file = NULL;
    input->bufferLength = 0;
    input->dmb = new Dmb();
    *args = input;

    return 0;
}


int dabInputDmbFileOpen(void* args, const char* inputName)
{
    int returnCode = 0;
    dabInputDmbFileData* input = (dabInputDmbFileData*)args;

    input->file = fopen(inputName, "r");
    if (input->file == NULL) {
        perror(inputName);
        returnCode = -1;
    }

    return returnCode;;
}


int dabInputDmbFileRead(dabInputOperations* ops, void* args, void* buffer, int size)
{
    int nbBytes = 0;
    dabInputDmbFileData* input = (dabInputDmbFileData*)args;
    dabInputFifoStats* stats = (dabInputFifoStats*)&input->stats;

    input->stats.frameRecords[input->stats.frameCount].curSize = 0;
    input->stats.frameRecords[input->stats.frameCount].maxSize = size;

    if (input->bufferLength == 0) {
        input->bufferLength = fread(input->buffer, 188, 1, input->file);
    }
/*    while ((nbBytes = writePacket(input->packet->getData(),
                    input->packet->getLength(), buffer, size, input->info))
            != 0) {
        input->stats.frameRecords[input->stats.frameCount].curSize = nbBytes;
        input->socket->receive(*input->packet);
    }*/
    while ((nbBytes = input->dmb->encode(input->buffer,
                    input->bufferLength * 188, buffer, size))
            != 0) {
        input->stats.frameRecords[input->stats.frameCount].curSize = nbBytes;
        input->bufferLength = fread(input->buffer, 188, 1, input->file);
        if (input->bufferLength == 0) {
            etiLog.log(info, "reach end of file -> rewinding\n");
            if (fseek(input->file, 0, SEEK_SET) == 0) {
                input->bufferLength = fread(input->buffer, 188, 1, input->file);
            }
        }
        //++mpgFrameNb;
    }
    //++dmbFrameNb;

    if (input->bufferLength != 0) {
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


int dabInputDmbFileClose(void* args)
{
    dabInputDmbFileData* input = (dabInputDmbFileData*)args;

    if (input->file != NULL) {
        if (fclose(input->file)) {
            perror("");
            return -1;
        }
    }

    return 0;
}


int dabInputDmbFileClean(void** args)
{
    dabInputDmbFileData* input = (dabInputDmbFileData*)(*args);
    dabInputDmbFileClose(args);
    delete input->dmb;
    delete input;
    return 0;
}


#endif //HAVE_FORMAT_DMB
