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

#include "dabInputRawFifo.h"

#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>

#ifndef _WIN32
#   define O_BINARY 0
#endif


#ifdef HAVE_FORMAT_RAW
#   ifdef HAVE_INPUT_FILE


struct dabInputOperations dabInputRawFifoOperations = {
    dabInputRawFifoInit,
    dabInputRawFifoOpen,
    dabInputRawFifoSetbuf,
    dabInputRawFifoRead,
    NULL,
    NULL,
    dabInputRawFifoReadFrame,
    dabInputSetbitrate,
    dabInputRawFifoClose,
    dabInputRawFifoClean,
    dabInputRawFifoRewind
};


int dabInputRawFifoInit(void** args)
{
    dabInputRawFifoData* data = new dabInputRawFifoData;
    data->file = -1;
    data->buffer = NULL;
    data->bufferSize = 0;
    data->bufferOffset = 0;

    *args = data;
    return 0;
}


int dabInputRawFifoOpen(void* args, const char* filename)
{
    dabInputRawFifoData* data = (dabInputRawFifoData*)args;
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

    return 0;
}


int dabInputRawFifoSetbuf(void* args, int size)
{
    dabInputRawFifoData* data = (dabInputRawFifoData*)args;

    if (size <= 0) {
        return size;
    }

    if (data->bufferSize != size) {
        if (data->buffer != NULL) {
            delete[] data->buffer;
        }
        data->buffer = new uint8_t[size];
        data->bufferSize = size;
        data->bufferOffset = 0;
    }

    return 0;
}


int dabInputRawFifoRead(void* args, void* buffer, int size)
{
    dabInputRawFifoData* data = (dabInputRawFifoData*)args;

    return read(data->file, buffer, size);
}


int dabInputRawFifoReadFrame(dabInputOperations* ops, void* args,
        void* buffer, int size)
{
    dabInputRawFifoData* data = (dabInputRawFifoData*)args;
    int result;

    result = ops->read(args, data->buffer + data->bufferOffset, size - data->bufferOffset);
    if (result == -1) {
        etiLog.log(alert, "ERROR: Can't read fifo\n");
        perror("");
        return -1;
    }

    if (result + data->bufferOffset < size) {
        data->bufferOffset += result;

        etiLog.log(info, "reach end of fifo -> rewinding\n");
        if (ops->rewind(args) == -1) {
            etiLog.log(alert, "ERROR: Can't rewind fifo\n");
            return -1;
        }

        result = ops->read(args, data->buffer + data->bufferOffset, size - data->bufferOffset);
        if (result == -1) {
            etiLog.log(alert, "ERROR: Can't read fifo\n");
            perror("");
            return -1;
        }

        if (result < size) {
            etiLog.log(alert, "ERROR: Not enought data in fifo\n");
            return 0;
        }
    }

    memcpy(buffer, data->buffer, size);
    data->bufferOffset = 0;

    return size;
}


int dabInputRawFifoClose(void* args)
{
    dabInputRawFifoData* data = (dabInputRawFifoData*)args;
    
    if (data->file != -1) {
        close(data->file);
        data->file = -1;
    }
    return 0;
}


int dabInputRawFifoClean(void** args)
{
    dabInputRawFifoData* data = (dabInputRawFifoData*)*args;
    if (data->buffer != NULL) {
        delete[] data->buffer;
    }
    delete data;
    return 0;
}


int dabInputRawFifoRewind(void* args)
{
    dabInputRawFifoData* data = (dabInputRawFifoData*)args;
    return lseek(data->file, 0, SEEK_SET);
}


#   endif
#endif
