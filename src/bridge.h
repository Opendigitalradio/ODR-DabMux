/*
   Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)
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

#ifndef _BRIDGE
#define _BRIDGE

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include <stdio.h>


#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#  ifdef _DEBUG
     extern int bridgeVerbosity;
#  endif // _DEBUG
#else
#  ifndef DEBUG
#   ifndef NDEBUG
#    define NDEBUG
#   endif
#  else
     extern int bridgeVerbosity;
#  endif // DEBUG
#endif // _WIN32

struct bridgeStats {
	unsigned long frames;		// Number of frames analyzed
	unsigned long valids;		// Nb of frames with a good magic number
	unsigned long invalids;		// Nb of frames with a good magic number
	unsigned long bytes;		// Nb of data bytes
	unsigned long packets;		// Nb of packets found
	unsigned long errors;
	unsigned long missings;
	unsigned long dropped;
	unsigned long crc;		// Nb of crc errors
	unsigned long overruns;		// Nb of packet too big
};


struct bridgeHdr {
    unsigned short size;
    unsigned short seqNb;
    unsigned short crc;
};


struct bridgeInfo {
    // Tx
    int transmitted;        // Nb bytes written
    int offset;             // Offset of the next byte to write
    // Rx
    int received;
    int pos;
    int state;
    unsigned short lastSeq;
    unsigned short sync;
    char initSeq;
    // General
    struct bridgeHdr header;
    struct bridgeStats stats;
};



void dump(void* data, int size, FILE* stream);

/*
 * Example of usae:
 * if (data.length == 0)
 *     read(data)
 * while (writePacket() != 0)
 *     read(read)
 * ...
 */
int writePacket(void* dataIn, int sizeIn, void* dataOut, int sizeOut, struct bridgeInfo* info);

int getPacket(void* dataIn, int sizeIn, void* dataOut, int sizeOut, struct bridgeInfo* info, char async);

void bridgeInitInfo(struct bridgeInfo* info);
struct bridgeStats getStats(struct bridgeInfo* info);
void resetStats(struct bridgeInfo* info);
void printStats(struct bridgeInfo* info, FILE* out);

#ifdef __cplusplus
}
#endif

#endif // _BRIDGE
