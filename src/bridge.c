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

#include <stdio.h>
#ifdef _WIN32
#  include <winsock2.h>
#else
#  include <netinet/in.h>
#endif // _WIN32
#include <string.h>
#include "bridge.h"
#include "crc.h"

#include <assert.h>
#include "PcDebug.h"

#ifdef _WIN32
#  ifdef _DEBUG
     int bridgeVerbosity = 0;
#  endif
#else
#  ifdef DEBUG
     int bridgeVerbosity = 0;
#  endif
#endif

void printStats(struct bridgeInfo* info, FILE* out)
{
    struct bridgeStats stats = getStats(info);
    fprintf(out, "frames     : %lu\n", stats.frames);
    fprintf(out, " valids    : %lu\n", stats.valids);
    fprintf(out, " invalids  : %lu\n", stats.invalids);
    fprintf(out, " bytes     : %lu\n", stats.bytes);
    fprintf(out, " packets   : %lu\n", stats.packets);
    fprintf(out, " errors    : %lu\n", stats.errors);
    fprintf(out, "  missings : %lu\n", stats.missings);
    fprintf(out, "  dropped  : %lu\n", stats.dropped);
    fprintf(out, "  crc      : %lu\n", stats.crc);
    fprintf(out, "  overruns : %lu\n", stats.overruns);
}


void resetStats(struct bridgeInfo* info)
{
    memset(&info->stats, 0, sizeof(info->stats));
}


struct bridgeStats getStats(struct bridgeInfo* info)
{
    return info->stats;
}


void bridgeInitInfo(struct bridgeInfo* info)
{
    memset(info, 0, sizeof(*info));
    info->transmitted = -8;
};


int writePacket(void* dataIn, int sizeIn, void* dataOut, int sizeOut,
        struct bridgeInfo* info)
{
    static struct bridgeHdr header = { 0 };

    PDEBUG4_VERBOSE(1, bridgeVerbosity, "writePacket\n sizeIn: %i, sizeOut: %i, "
            "offset: %i, transmitted: %i\n",
            sizeIn, sizeOut, info->offset, info->transmitted);

    assert(info->transmitted < sizeIn);

    if ((info->offset == 0) && (sizeIn > 0)) {
        ((unsigned short*)dataOut)[0] = 0xb486;
        info->offset = 2;
    }
    if (sizeIn == 0) {
        memset((unsigned char*)dataOut + info->offset, 0, sizeOut - info->offset);
        info->offset = 0;
        info->transmitted = -8;
        PDEBUG1_VERBOSE(1, bridgeVerbosity, " return %i (sizeIn == 0)\n",
                sizeOut);
        return 0;
    }

    while (info->offset < sizeOut) {
        switch (info->transmitted) {
        case (-8):
            ((unsigned char*)dataOut)[info->offset++] = 0xcb;
            ++info->transmitted;
            break;
        case (-7):
            ((unsigned char*)dataOut)[info->offset++] = 0x28;
            ++info->transmitted;
            break;
        case (-6):
            header.size = htons((unsigned short)sizeIn);
            header.crc = htons((unsigned short)(crc16(0xffff, &header, 4) ^ 0xffff));
            ((unsigned char*)dataOut)[info->offset++] = ((char*)&header)[0];
            ++info->transmitted;
            break;
        case (-5):
            ((unsigned char*)dataOut)[info->offset++] = ((char*)&header)[1];
            ++info->transmitted;
            break;
        case (-4):
            ((unsigned char*)dataOut)[info->offset++] = ((char*)&header)[2];
            ++info->transmitted;
            break;
        case (-3):
            ((unsigned char*)dataOut)[info->offset++] = ((char*)&header)[3];
            ++info->transmitted;
            break;
        case (-2):
            ((unsigned char*)dataOut)[info->offset++] = ((char*)&header)[4];
            ++info->transmitted;
            break;
        case (-1):
            ((unsigned char*)dataOut)[info->offset++] = ((char*)&header)[5];
            ++info->transmitted;
            header.seqNb = htons((unsigned short)(ntohs(header.seqNb) + 1));
            break;
        default:
            ((unsigned char*)dataOut)[info->offset++] =
                ((unsigned char*)dataIn)[info->transmitted++];
            if (info->transmitted == sizeIn) {
                PDEBUG2_VERBOSE(1, bridgeVerbosity,
                        " Packet done, %i bytes at offset %i\n",
                        info->transmitted, info->offset);
                PDEBUG1_VERBOSE(1, bridgeVerbosity,
                        " return %i (sizeIn == transmitted)\n", info->offset);
                info->transmitted = -8;
                return info->offset;
            }
        }
    }

    PDEBUG1_VERBOSE(1, bridgeVerbosity, " return %i (offset >= sizeOut)\n",
            info->offset);
    info->offset = 0;
    return 0;
}


int getPacket(void* dataIn, int sizeIn, void* dataOut, int sizeOut,
        struct bridgeInfo* info, char async)
{
    unsigned char* in = (unsigned char*)dataIn;
    unsigned char* out = (unsigned char*)dataOut;
    unsigned char ch;
    unsigned short crc;
    unsigned short diff;

    PDEBUG3_VERBOSE(1, bridgeVerbosity,
            "getPacket\n pos\t%i\n state\t%i\n received\t%i\n",
            info->pos, info->state, info->received);

    if (info->pos == 0) {
        ++info->stats.frames;
        if (((unsigned short*)dataIn)[0] != 0xb486) {
            if (((unsigned short*)dataIn)[0] != 0) {
                ++info->stats.invalids;
                printf("WARNING: processing frame with invalid magic "
                        "number!\n");
            } else {
                PDEBUG0_VERBOSE(1, bridgeVerbosity,
                        "getPacket: not a valid frame\n");
                return 0;
            }
        } else {
            PDEBUG0_VERBOSE(2, bridgeVerbosity, "Valid frame\n");
            info->pos += 2;
            ++info->stats.valids;
        }
        info->stats.bytes += sizeIn;
    }
    while (info->pos < sizeIn) {
        ch = in[info->pos++];
        switch (info->state) {
        case 0:  // sync search
            info->sync <<= 8;
            info->sync |= ch;
            if (info->sync == 0xcb28) {
                PDEBUG0_VERBOSE(2, bridgeVerbosity, "Sync found\n");
                ++info->stats.packets;
                info->received = 0;
                info->state = 1;
            }
            if (info->sync == 0) {		// Padding
                info->pos = 0;
                return 0;
            }
            break;
        case 1:  // header search
            ((char*)&info->header)[info->received++] = ch;
            if (info->received == sizeof(struct bridgeHdr)) {
                PDEBUG0_VERBOSE(2, bridgeVerbosity, "Header found\n");
                out = (unsigned char*)dataOut;
                info->received = 0;
                info->state = 2;
                crc = crc16(0xffff, &info->header, 4);
                crc ^= 0xffff;
                info->header.size = ntohs(info->header.size);
                info->header.seqNb = ntohs(info->header.seqNb);
                info->header.crc = ntohs(info->header.crc);
                PDEBUG4_VERBOSE(2, bridgeVerbosity,
                        " size\t%i\n seq\t%i\n crc\t0x%.4x (0x%.4x)\n",
                        info->header.size, info->header.seqNb,
                        info->header.crc, crc);
                if (crc != info->header.crc) {
                    PDEBUG0_VERBOSE(2, bridgeVerbosity, "CRC error\n");
                    ++info->stats.errors;
                    ++info->stats.crc;
                    info->state = 0;
                    if (info->pos < sizeof(struct bridgeHdr) + 2 + 2) {
                        info->pos = 2;
                    }
                } else {
                    if (!info->initSeq) {
                        info->lastSeq = info->header.seqNb;
                        info->initSeq = 1;
                    } else {
                        if (info->header.seqNb > info->lastSeq) {
                            diff = (info->header.seqNb - info->lastSeq) - 1;
                        } else {
                            diff = ((short)info->lastSeq -
                                    (short)info->header.seqNb) - 1;
                        }
                        info->stats.errors += diff;
                        info->stats.missings += diff;
                        info->lastSeq = info->header.seqNb;
                    }
                }
            }
            break;
        case 2:  // data
            out[info->received++] = ch;
            if (info->received == info->header.size) {
                PDEBUG0_VERBOSE(2, bridgeVerbosity, "data found\n");
                info->state = 0;
                return info->received;
            }
            if (info->received == sizeOut) {
                PDEBUG1_VERBOSE(1, bridgeVerbosity, "To much data: %i\n",
                        info->received);
                ++info->stats.errors;
                ++info->stats.overruns;
                info->sync = 0;
                info->state = 0;
                return -1;
            }
            break;
        case 3:  // Padding or sync
            if (ch == 0) {			// Padding
                info->pos = 0;
                return 0;
            }
            if (ch != 0xcb) {		// error
                info->sync = ch;
                info->state = 0;
            } else {
                info->state = 4;
            }
            break;
        case 4: // Low byte sync
            if (ch != 28) {			// error
                info->sync <<= 8;
                info->sync |= ch;
                info->state = 0;
            } else {
                info->state = 2;
            }
            break;
        }
    }
    info->pos = 0;
    return 0;
}


void dump(void* data, int size, FILE* stream)
{
    int i;
    fprintf(stream, "%i bytes\n", size);
    for (i = 0; i < size; ++i) {
        fprintf(stream, " 0x%.2x", ((unsigned char*)data)[i]);
        if (i % 8 == 7)
            fprintf(stream, "\n");
    }
    fprintf(stream, "\n");
}


#ifdef BRIDGE_TEST
#include <stdlib.h>


int test(const unsigned char* data)
{
    unsigned char bridgeSize = data[0];
    unsigned char nbInput = data[1];
    unsigned char nbBridge = 1;
    struct bridgeInfo info;

    int i, j;
    int index = 0;
    int max = 0;
    int nbBytes;

    unsigned char** inputData;
    unsigned char** bridgeData;
    unsigned char* outputData;

    inputData =  malloc(nbInput * 4);
    bridgeData = malloc(nbBridge * 4);
    for (i = 0; i < nbInput; ++i) {
        if (data[i + 2] > 0)
            inputData[i] = malloc(data[i + 2]);
        if (data[i + 2] > max) {
            max = data[i + 2];
        }
        for (j = 0; j < data[i + 2]; ++j) {
            inputData[i][j] = index++;
        }
    }
    bridgeData[0] = malloc(bridgeSize);
    memset(bridgeData[0], 0, bridgeSize);
    outputData = malloc(max);
    bridgeInitInfo(&info);

    // Write packets
    index = 0;
    while (1) {
        if (data[index + 2] == 0) {
            if (++index == nbInput)
                break;
        }
        while ((nbBytes = writePacket(inputData[index], data[index + 2],
                        bridgeData[nbBridge - 1], bridgeSize, &info))
                != 0) {
            if (++index == nbInput) {
                break;
            }
        }
        if (index == nbInput)
            break;
        // TODO check null
        bridgeData = realloc(bridgeData, (++nbBridge) * 4);
        bridgeData[nbBridge - 1] = malloc(bridgeSize);
        memset(bridgeData[nbBridge - 1], 0, bridgeSize);
    }
//    if (nbBytes != bridgeSize) {
        writePacket(NULL, 0, bridgeData[nbBridge - 1], bridgeSize, &info);
//    }

    // read packets
    index = 0;
    for (i = 0; i < nbBridge; ++i) {
        while ((nbBytes = getPacket(bridgeData[i], bridgeSize, outputData, max,
                        &info, 0)) != 0) {
            while (data[index + 2] == 0) {
                ++index;
            }
            if (nbBytes != data[index + 2]) {
                printf("FAILED\n");
                printf("Invalid size at bridge %i, data %i: %i != %i\n",
                        i, index, nbBytes, data[index + 2]);
                for (i = 0; i < nbInput; ++i) {
                    printf("Input %i: ", i);
                    dump(inputData[i], data[i + 2], stdout);
                }
                for (i = 0; i < nbBridge; ++i) {
                    printf("Bridge %i: ", i);
                    dump(bridgeData[i], bridgeSize, stdout);
                }
                printf("Output %i: ", index);
                dump(outputData, nbBytes, stdout);
                return -1;
            }
            if (memcmp(outputData, inputData[index], data[index + 2]) != 0) {
                printf("FAILED\n");
                printf("output != input\n");
                for (i = 0; i < nbInput; ++i) {
                    printf("Input %i: ", i);
                    dump(inputData[i], data[i + 2], stdout);
                }
                for (i = 0; i < nbBridge; ++i) {
                    printf("Bridge %i: ", i);
                    dump(bridgeData[i], bridgeSize, stdout);
                }
                printf("Output %i: ", index);
                dump(outputData, nbBytes, stdout);
            }
            ++index;
        }
    }

    printf("SUCCESS\n");

    for (i = 0; i < nbInput; ++i) {
        if (data[i + 2] > 0)
            free(inputData[i]);
    }
    free(inputData);
    free(outputData);
    for (i = 0; i < nbBridge; ++i) {
        free(bridgeData[i]);
    }
    free(bridgeData);

    return -1;
}


int main(int argc, char* argv[])
{
    int i;
    // test: bridgesize, nbinput [, input1, input2, ... ]
    const unsigned char complete[] = { 32, 1, 16 };
    const unsigned char split[] = { 32, 1, 48 };
    const unsigned char twice[] = {32, 2, 8, 4 };
    const unsigned char secondSplit[] = { 32, 2, 16, 16 };
    const unsigned char headerSplit[][4] = {
        { 32, 2, 23, 16 },
        { 32, 2, 22, 16 },
        { 32, 2, 21, 16 },
        { 32, 2, 20, 16 },
        { 32, 2, 19, 16 },
        { 32, 2, 18, 16 },
        { 32, 2, 17, 16 }
    };
    const unsigned char two[] = { 32, 3, 16, 0, 16 };
    const unsigned char doubleSplit[] = { 32, 2, 32, 32 };
    const unsigned char full[] = { 32, 2, 24, 12 };
    const unsigned char empty[] = { 32, 3, 0, 0, 5 };

#ifdef _WIN32
  #ifdef _DEBUG
    bridgeVerbosity = argc - 1;
  #endif // DEBUG
#else
  #ifdef DEBUG
    bridgeVerbosity = argc - 1;
  #endif // DEBUG
#endif // _WIN32

    printf("Complete:     ");
    test(complete);
    //	printStats(stdout);
    fflush(stdout);

    printf("split:        ");
    test(split);
    //	printStats(stdout);
    fflush(stdout);

    printf("twice:        ");
    test(twice);
    //	printStats(stdout);
    fflush(stdout);

    printf("second split: ");
    test(secondSplit);
    //	printStats(stdout);
    fflush(stdout);

    for (i = 0; i < sizeof(headerSplit) / sizeof(headerSplit[0]); ++i) {
        printf("headerSplit%i: ", i);
        test(headerSplit[i]);
        //	printStats(stdout);
        fflush(stdout);
    }

    printf("two:          ");
    test(two);
    //	printStats(stdout);
    fflush(stdout);

    printf("doubleSplit:  ");
    test(doubleSplit);
    //	printStats(stdout);
    fflush(stdout);

    printf("full:         ");
    test(full);
    //	printStats(stdout);
    fflush(stdout);

    printf("empty:        ");
    test(empty);
    //	printStats(stdout);
    fflush(stdout);

    return 0;
}

#endif // BRIDGE_TEST
