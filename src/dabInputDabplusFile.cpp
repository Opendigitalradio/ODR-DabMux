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

#include "dabInputDabplusFile.h"
#include "dabInput.h"

#include <stdio.h>
#include <string.h>
#include <fcntl.h>

#ifndef _WIN32
#   define O_BINARY 0
#endif


#ifdef HAVE_FORMAT_DABPLUS
#   ifdef HAVE_INPUT_FILE


struct dabInputOperations dabInputDabplusFileOperations = {
    dabInputDabplusFileInit,
    dabInputDabplusFileOpen,
    dabInputSetbuf,
    dabInputDabplusFileRead,
    NULL,
    NULL,
    dabInputDabplusFileReadFrame,
    dabInputSetbitrate,
    dabInputDabplusFileClose,
    dabInputDabplusFileClean,
    dabInputDabplusFileRewind
};


int dabInputDabplusFileInit(void** args)
{
    dabInputDabplusFileData* data = new dabInputDabplusFileData;
    data->file = -1;
    data->buffer = NULL;
    data->bufferSize = 0;
    data->bufferIndex = 0;

    *args = data;
    return 0;
}


int dabInputDabplusFileOpen(void* args, const char* filename)
{
    dabInputDabplusFileData* data = (dabInputDabplusFileData*)args;
    data->file = open(filename, O_RDONLY | O_BINARY);
    if (data->file == -1) {
        perror(filename);
        return -1;
    }

    return 0;
}


int dabInputDabplusFileRead(void* args, void* buffer, int size)
{
    dabInputDabplusFileData* data = (dabInputDabplusFileData*)args;
    if (data->bufferSize != size * 5) {
        if (data->buffer == NULL) {
            delete[] data->buffer;
        }
        data->buffer = new uint8_t[size * 5];
        data->bufferSize = size * 5;
        data->bufferIndex = 0;
    }

    if (data->bufferIndex + size > data->bufferSize) {
        int ret = read(data->file, data->buffer, data->bufferSize);
        if (ret != data->bufferSize) {
            if (ret != 0) {
                etiLog.print(TcpLog::CRIT, "ERROR: Incomplete DAB+ frame!\n");
            }
            return 0;
        }
        data->bufferIndex = 0;
    }

    memcpy(buffer, &data->buffer[data->bufferIndex], size);
    data->bufferIndex += size;

    return size;
}


int dabInputDabplusFileReadFrame(dabInputOperations* ops, void* args,
        void* buffer, int size)
{
    //dabInputDabplusFileData* data = (dabInputDabplusFileData*)args;
    int result;
    uint8_t* dataOut = reinterpret_cast<uint8_t*>(buffer);

    result = ops->read(args, dataOut, size);
    if (result == -1) {
        etiLog.print(TcpLog::CRIT, "ERROR: Can't read file\n");
        perror("");
        return -1;
    }
    if (result < size) {
        int sizeOut = result;
        etiLog.print(TcpLog::NOTICE, "reach end of file -> rewinding\n");
        if (ops->rewind(args) == -1) {
            etiLog.print(TcpLog::CRIT, "ERROR: Can't rewind file\n");
            return -1;
        }

        result = ops->read(args, dataOut + sizeOut, size - sizeOut);
        if (result == -1) {
            etiLog.print(TcpLog::CRIT, "ERROR: Can't read file\n");
            perror("");
            return -1;
        }

        if (result < size) {
            etiLog.print(TcpLog::CRIT, "ERROR: Not enought data in file\n");
            return -1;
        }
    }

    return size;
}


int dabInputDabplusFileClose(void* args)
{
    dabInputDabplusFileData* data = (dabInputDabplusFileData*)args;
    if (data->file != -1) {
        close(data->file);
    }
    return 0;
}


int dabInputDabplusFileClean(void** args)
{
    dabInputDabplusFileData* data = (dabInputDabplusFileData*)*args;
    if (data->buffer != NULL) {
        delete[] data->buffer;
    }
    delete data;
    return 0;
}


int dabInputDabplusFileRewind(void* args)
{
    dabInputDabplusFileData* data = (dabInputDabplusFileData*)args;
    return lseek(data->file, 0, SEEK_SET);
}


#   endif
#endif
