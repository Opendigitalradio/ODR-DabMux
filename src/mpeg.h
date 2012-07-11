/*
   Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)
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

#ifndef _MPEG
#define _MPEG

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#ifdef _WIN32
#   include <stddef.h>
#   include <basetsd.h>
#   include <io.h>

#   define ssize_t SSIZE_T
#else
#   include <unistd.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    unsigned long sync1:8;

    unsigned long protection:1;
    unsigned long layer:2;
    unsigned long id:2;
    unsigned long sync2:3;

    unsigned long priv:1;
    unsigned long padding:1;
    unsigned long samplingrate:2;
    unsigned long bitrate:4;

    unsigned long emphasis:2;
    unsigned long original:1;
    unsigned long copyright:1;
    unsigned long mode:2;
    unsigned long channel:2;
} mpegHeader;


extern short getMpegBitrate(void* data);
extern int getMpegFrequency(void* data);
int getMpegFrameLength(mpegHeader* header);
ssize_t readData(int file, void* data, size_t size, unsigned int tries);

#define MPEG_BUFFER_OVERFLOW    -2
#define MPEG_FILE_EMPTY         -3
#define MPEG_FILE_ERROR         -4
#define MPEG_SYNC_NOT_FOUND     -5
#define MPEG_INVALID_FRAME      -6
#define MPEG_BUFFER_UNDERFLOW   -7
int readMpegHeader(int file, void* data, int size);
int readMpegFrame(int file, void* data, int size);

#ifdef __cplusplus
}
#endif

#endif // _MPEG
