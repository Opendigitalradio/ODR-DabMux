/*
   Copyright (C) 2009,2011 Her Majesty the Queen in Right of Canada
   (Communications Research Center Canada)
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

#include "dabInputFile.h"

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#ifndef _WIN32
#   define O_BINARY 0
#endif


int dabInputFileInit(void** args)
{
    dabInputFileData* data = new dabInputFileData;
    data->file = -1;
    data->parity = false;
    data->packetLength = 0;
    data->packetData = new unsigned char[96];
    data->enhancedPacketData = NULL;
    data->enhancedPacketLength = 0;
    data->enhancedPacketWaiting = 0;

    *args = data;
    return 0;
}


int dabInputFileOpen(void* args, const char* filename)
{
    dabInputFileData* data = (dabInputFileData*)args;
    data->file = open(filename, O_RDONLY | O_BINARY);
    if (data->file == -1) {
        perror(filename);
        return -1;
    }
    return 0;
}


int dabInputFileRead(void* args, void* buffer, int size)
{
    dabInputFileData* data = (dabInputFileData*)args;
    return read(data->file, buffer, size);
}


int dabInputFileClose(void* args)
{
    dabInputFileData* data = (dabInputFileData*)args;
    if (data->file != -1) {
        close(data->file);
    }
    return 0;
}


int dabInputFileClean(void** args)
{
    dabInputFileData* data = (dabInputFileData*)*args;
    if (data->packetData != NULL) {
        delete[] data->packetData;
    }
    if (data->enhancedPacketData != NULL) {
        for (int i = 0; i < 12; ++i) {
            if (data->enhancedPacketData[i] != NULL) {
                delete[] data->enhancedPacketData[i];
            }
        }
        delete[] data->enhancedPacketData;
    }
    delete data;
    return 0;
}


int dabInputFileRewind(void* args)
{
    dabInputFileData* data = (dabInputFileData*)args;
    return lseek(data->file, 0, SEEK_SET);
}


