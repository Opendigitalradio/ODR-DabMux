/*
   Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Her Majesty the Queen in
   Right of Canada (Communications Research Center Canada)

   Copyright (C) 2013 Matthias P. Braendli
   http://mpb.li

   Fifo output is very similar to file, except it doesn't seek
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
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <limits.h>
#include "dabOutput.h"


int DabOutputFifo::Write(void* buffer, int size)
{
    uint8_t padding[6144];

    switch (this->type_) {
        case ETI_FILE_TYPE_FRAMED:
            if (this->nbFrames_ == 0) {
                uint32_t nbFrames = (uint32_t)-1;
                // Writing nb frames
                if (write(this->file_, &nbFrames, 4) == -1)
                    goto FIFO_WRITE_ERROR;
            }
        case ETI_FILE_TYPE_STREAMED:
            // Writting frame length
            if (write(this->file_, &size, 2) == -1)
                goto FIFO_WRITE_ERROR;
            // Appending data
            if (write(this->file_, buffer, size) == -1)
                goto FIFO_WRITE_ERROR;
            break;
        case ETI_FILE_TYPE_RAW:
            // Appending data
            if (write(this->file_, buffer, size) == -1)
                goto FIFO_WRITE_ERROR;
            // Appending padding
            memset(padding, 0x55, 6144 - size);
            if (write(this->file_, padding, 6144 - size) == -1)
                goto FIFO_WRITE_ERROR;
            break;
        default:
            etiLog.log(error, "File type is not supported.\n");
            return -1;
    }

    return size;

FIFO_WRITE_ERROR:
    perror("Error while writting to file");
    return -1;
}

