/*
   Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)
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

#include "mpeg.h"

#include <stdio.h>
#include <errno.h>


static const short bitrateArray[4][4][16] = {
    { // MPEG 2.5
        {  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
           -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1 }, // layer invalid
        {   0,   8,  16,  24,  32,  40,  48,  56,
           64,  80,  96, 112, 128, 144, 160,  -1 }, // layer 3
        {   0,   8,  16,  24,  32,  40,  48,  56,
           64,  80,  96, 112, 128, 144, 160,  -1 }, // layer 2
        {   0,  32,  48,  56,  64,  80,  96, 112,
          128, 144, 160, 176, 192, 224, 256,  -1 } // layer 1
    },
    {
        {  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
           -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1 }, // MPEG invalid
        {  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
           -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1 },
        {  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
           -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1 },
        {  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
           -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1 }
    },
    {
        {  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
           -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1 }, // MPEG 2
        {   0,   8,  16,  24,  32,  40,  48,  56,
           64,  80,  96, 112, 128, 144, 160,  -1 },
        {   0,   8,  16,  24,  32,  40,  48,  56,
           64,  80,  96, 112, 128, 144, 160,  -1 },
        {   0,  32,  48,  56,  64,  80,  96, 112,
          128, 144, 160, 176, 192, 224, 256,  -1 }
    },
    {
        {  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
           -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1 }, // MPEG 1
        {   0,  32,  40,  48,  56,  64,  80,  96,
          112, 128, 160, 192, 224, 256, 320,  -1 },
        {   0,  32,  48,  56,  64,  80,  96, 112,
          128, 160, 192, 224, 256, 320, 384,  -1 },
        {   0,  32,  64,  96, 128, 160, 192, 224,
          256, 288, 320, 352, 384, 416, 448,  -1 }
    }
};


static const int samplingrateArray[4][4] = {
    { 11025, 12000, 8000, 0 },  // MPEG 2.5
    { -1, -1, -1, -1 },         // MPEG invalid
    { 22050, 24000, 16000, 0 }, // MPEG 2
    { 44100, 48000, 32000, 0 }  // MPEG 1
};


short getMpegBitrate(void* data)
{
    mpegHeader* header = (mpegHeader*)data;
    return bitrateArray[header->id][header->layer][header->bitrate];
}


int getMpegFrequency(void* data)
{
    mpegHeader* header = (mpegHeader*)data;
    return samplingrateArray[header->id][header->samplingrate];
}


int getMpegFrameLength(mpegHeader* header)
{
    short bitrate = getMpegBitrate(header);
    int samplingrate = getMpegFrequency(header);
    int framelength = -1;


    if (bitrate <= 0) {
        return -1;
    }
    if (samplingrate <= 0) {
        return -1;
    }

    switch (header->layer) {
    case 1:  // layer III
    case 2:  // layer II
        framelength = (int)144000 * bitrate / samplingrate + header->padding;
        break;
    case 3:  // layer I
        framelength = (12 * bitrate / samplingrate + header->padding) * 4;
        break;
    default:
        framelength = -1;
    }
    return framelength;
}


/**
 *  This function replace the read function by trying many times a reading.
 *  It tries to read until all bytes are read. Very useful when reading from a
 *  pipe because sometimes 2 pass is necessary to read all bytes.
 *  @param file     File descriptor.
 *  @param data     Address of the buffer to write data into.
 *  @param size     Number of bytes to read.
 *  @param tries    Max number of tries to read.
 *  @return         Same as read function:
 *                  Nb of bytes read.
 *                  -1 if error.
 */
ssize_t readData(int file, void* data, size_t size, unsigned int tries)
{
    ssize_t result;
    size_t offset = 0;
    if (size == 0) return 0;
    if (tries == 0) return 0;
    result = read(file, data, size);
    if (result == -1) {
        if (errno == EAGAIN) {
            return readData(file, data, size, tries - 1);
        }
        return -1;
    }
    offset = result;
    size -= offset;
    data = (char*)data + offset;
    result = readData(file, data, size, tries - 1);
    if (result == -1) {
        return -1;
    }
    offset += result;
    return offset;
}


int readMpegHeader(int file, void* data, int size)
{
    // Max byte reading for searching sync
    unsigned int flagmax = 0;
    unsigned long* syncword = (unsigned long*)data;
    int result;

    if (size < 4) return MPEG_BUFFER_OVERFLOW;
    // Mpeg header reading
    result = readData(file, data, 4, 2);
    if (result == 0) return MPEG_FILE_EMPTY;
    if (result == -1) {
        fprintf(stderr, "header\n");
        return MPEG_FILE_ERROR;
    }
    if (result < 4) {
        return MPEG_BUFFER_UNDERFLOW;
    }
    while ((*syncword & 0xe0ff) != 0xe0ff) {
        *syncword >>= 8;
        result = readData(file, (char*)data + 3, 1, 2);
        if (result == 0) {
            return MPEG_FILE_EMPTY;
        } else if (result == -1) {
            return MPEG_FILE_ERROR;
        }
        if (++flagmax > 1200) {
            return MPEG_SYNC_NOT_FOUND;
        }
    } 
    return 4;
}


int readMpegFrame(int file, void* data, int size)
{
    mpegHeader* header = (mpegHeader*)data;
    int framelength;
    int result;

    framelength = getMpegFrameLength(header);
    if (framelength < 0) return MPEG_INVALID_FRAME;
    if (framelength > size) {
        //lseek(file, framelength - 4, SEEK_CUR);
        //return MPEG_BUFFER_OVERFLOW;
        result = readData(file, ((char*)data) + 4, size - 4, 2);
        if (size == 4) {
            lseek(file, framelength - size, SEEK_CUR);
            result = framelength - 4;
        }
    } else {
        result = readData(file, ((char*)data) + 4, framelength - 4, 2);
    }
    if (result == 0) return MPEG_FILE_EMPTY;
    if (result == -1) {
        return MPEG_FILE_ERROR;
    }
    if (result < framelength - 4) {
        return MPEG_BUFFER_UNDERFLOW;
    }
    return framelength;
}

int checkDabMpegFrame(void* data) {
    mpegHeader* header = (mpegHeader*)data;
    unsigned long* headerData = (unsigned long*)data;
    if ((*headerData & 0x0f0ffcff) == 0x0004fcff) return 0;
    if ((*headerData & 0x0f0ffcff) == 0x0004f4ff) return 0;
    if (getMpegFrequency(header) != 48000) {
        if (getMpegFrequency(header) != 24000) {
            return MPEG_FREQUENCY;
        }
    }
    if (header->padding != 0) {
        return MPEG_PADDING;
    }
    if (header->copyright != 0) {
        return MPEG_COPYRIGHT;
    }
    if (header->original != 0) {
        return MPEG_ORIGINAL;
    }
    if (header->emphasis != 0) {
        return MPEG_EMPHASIS;
    }
    return -1;
}

