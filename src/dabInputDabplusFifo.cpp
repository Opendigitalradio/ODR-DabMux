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

#include "dabInputDabplusFifo.h"
#include "dabInputDabplusFile.h"
#include "dabInputFifo.h"
#include "dabInput.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#ifndef _WIN32
#   define O_BINARY 0
#endif


#ifdef HAVE_FORMAT_DABPLUS
#   ifdef HAVE_INPUT_FILE


struct dabInputDabplusFifoData {
    void* fifoData;
    uint8_t* buffer;
    size_t bufferSize;
    size_t bufferIndex;
    size_t bufferOffset;
};


struct dabInputOperations dabInputDabplusFifoOperations = {
    dabInputDabplusFifoInit,
    dabInputDabplusFifoOpen,
    dabInputDabplusFifoSetbuf,
    dabInputDabplusFifoRead,
    dabInputDabplusFifoLock,
    dabInputDabplusFifoUnlock,
    dabInputDabplusFileReadFrame,
    dabInputSetbitrate,
    dabInputDabplusFifoClose,
    dabInputDabplusFifoClean,
    dabInputDabplusFifoRewind
};


int dabInputDabplusFifoInit(void** args)
{
    dabInputDabplusFifoData* data = new dabInputDabplusFifoData;

    dabInputFifoInit(&data->fifoData);
    data->buffer = NULL;
    data->bufferSize = 0;
    data->bufferIndex = 0;
    data->bufferOffset = 0;

    *args = data;

    return 0;
}


int dabInputDabplusFifoOpen(void* args, const char* filename)
{
    dabInputDabplusFifoData* data = (dabInputDabplusFifoData*)args;

    return dabInputFifoOpen(data->fifoData, filename);
}


int dabInputDabplusFifoSetbuf(void* args, int size)
{
    dabInputDabplusFifoData* data = (dabInputDabplusFifoData*)args;

    return dabInputFifoSetbuf(data->fifoData, size);
}


int dabInputDabplusFifoRead(void* args, void* buffer, int size)
{
    dabInputDabplusFifoData* data = (dabInputDabplusFifoData*)args;

    if (data->bufferSize != (size_t)size * 5) {
        if (data->buffer != NULL) {
            delete[] data->buffer;
        }
        data->buffer = new uint8_t[size * 5];
        data->bufferSize = size * 5;
        data->bufferIndex = 0;
    }

    if (data->bufferOffset < data->bufferSize) {
        int ret = dabInputFifoRead(data->fifoData,
                &data->buffer[data->bufferOffset],
                data->bufferSize - data->bufferOffset);
        if (ret < 0) {
            return ret;
        }
        data->bufferOffset += ret;
        if (data->bufferOffset != data->bufferSize) {
            etiLog.log(alert, "ERROR: Incomplete DAB+ frame!\n");
            return 0;
        }
    }

    memcpy(buffer, &data->buffer[data->bufferIndex], size);
    data->bufferIndex += size;
    if (data->bufferIndex >= data->bufferSize) {
        data->bufferIndex = 0;
        data->bufferOffset = 0;
    }
    return size;
}


int dabInputDabplusFifoLock(void* args)
{
    dabInputDabplusFifoData* data = (dabInputDabplusFifoData*)args;

    return dabInputFifoLock(data->fifoData);
}


int dabInputDabplusFifoUnlock(void* args)
{
    dabInputDabplusFifoData* data = (dabInputDabplusFifoData*)args;

    return dabInputFifoUnlock(data->fifoData);
}


int dabInputDabplusFifoReadFrame(dabInputOperations* ops, void* args,
        void* buffer, int size)
{
    return ops->read(args, buffer, size);
}


int dabInputDabplusFifoClose(void* args)
{
    dabInputDabplusFifoData* data = (dabInputDabplusFifoData*)args;

    return dabInputFifoClose(data->fifoData);
}


int dabInputDabplusFifoClean(void** args)
{
    dabInputDabplusFifoData* data = (dabInputDabplusFifoData*)args;

    dabInputFifoClean(&data->fifoData);
    delete data;

    return 0;
}


int dabInputDabplusFifoRewind(void* args)
{
    dabInputDabplusFifoData* data = (dabInputDabplusFifoData*)args;

    return dabInputFifoRewind(data->fifoData);
}


#   endif
#endif
