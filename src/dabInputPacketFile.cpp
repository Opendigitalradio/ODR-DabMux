/*
   Copyright (C) 2009, 2011 Her Majesty the Queen in Right of Canada
   (Communications Research Center Canada)
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

#include "dabInputPacketFile.h"
#include "dabInputFile.h"
#include "dabInputFifo.h"
#include "ReedSolomon.h"

#ifdef HAVE_FORMAT_PACKET
#   ifdef HAVE_INPUT_FILE


#include <string.h>
#include <stdio.h>
#include <errno.h>


#ifdef _WIN32
#   pragma pack(push, 1)
#endif
struct packetHeader {
    unsigned char addressHigh:2;
    unsigned char last:1;
    unsigned char first:1;
    unsigned char continuityIndex:2;
    unsigned char packetLength:2;
    unsigned char addressLow;
    unsigned char dataLength:7;
    unsigned char command;
}
#ifdef _WIN32
#   pragma pack(pop)
#else
__attribute((packed))
#endif
;


struct dabInputOperations dabInputPacketFileOperations = {
    dabInputFileInit,
    dabInputFileOpen,
    dabInputSetbuf,
    dabInputFileRead,
    NULL,
    NULL,
    dabInputPacketFileRead,
    dabInputSetbitrate,
    dabInputFileClose,
    dabInputFileClean,
    dabInputFileRewind
};


int dabInputPacketFileRead(dabInputOperations* ops, void* args, void* buffer,
        int size)
{
    dabInputFileData* data = (dabInputFileData*)args;
    unsigned char* dataBuffer = (unsigned char*)buffer;
    int written = 0;
    int length;
    packetHeader* header;
    int indexRow;
    int indexCol;

    while (written < size) {
        if (data->enhancedPacketWaiting > 0) {
            *dataBuffer = 192 - data->enhancedPacketWaiting;
            *dataBuffer /= 22;
            *dataBuffer <<= 2;
            *(dataBuffer++) |= 0x03;
            *(dataBuffer++) = 0xfe;
            indexCol = 188;
            indexCol += (192 - data->enhancedPacketWaiting) / 12;
            indexRow = 0;
            indexRow += (192 - data->enhancedPacketWaiting) % 12;
            for (int j = 0; j < 22; ++j) {
                if (data->enhancedPacketWaiting == 0) {
                    *(dataBuffer++) = 0;
                } else {
                    *(dataBuffer++) = data->enhancedPacketData[indexRow][indexCol];
                    if (++indexRow == 12) {
                        indexRow = 0;
                        ++indexCol;
                    }
                    --data->enhancedPacketWaiting;
                }
            }
            written += 24;
            if (data->enhancedPacketWaiting == 0) {
                data->enhancedPacketLength = 0;
            }
        } else if (data->packetLength != 0) {
            header = (packetHeader*)data->packetData;
            if (written + data->packetLength > (unsigned)size) {
                memset(dataBuffer, 0, 22);
                dataBuffer[22] = 0x60;
                dataBuffer[23] = 0x4b;
                length = 24;
            } else if (data->enhancedPacketData != NULL) {
                if (data->enhancedPacketLength + data->packetLength
                        > (12 * 188)) {
                    memset(dataBuffer, 0, 22);
                    dataBuffer[22] = 0x60;
                    dataBuffer[23] = 0x4b;
                    length = 24;
                } else {
                    memcpy(dataBuffer, data->packetData, data->packetLength);
                    length = data->packetLength;
                    data->packetLength = 0;
                }
            } else {
                memcpy(dataBuffer, data->packetData, data->packetLength);
                length = data->packetLength;
                data->packetLength = 0;
            }
            if (data->enhancedPacketData != NULL) {
                indexCol = data->enhancedPacketLength / 12;
                indexRow = data->enhancedPacketLength % 12;     // TODO Check if always 0
                for (int j = 0; j < length; ++j) {
                    data->enhancedPacketData[indexRow][indexCol] = dataBuffer[j];
                    if (++indexRow == 12) {
                        indexRow = 0;
                        ++indexCol;
                    }
                }
                data->enhancedPacketLength += length;
                if (data->enhancedPacketLength >= (12 * 188)) {
                    data->enhancedPacketLength = (12 * 188);
                    ReedSolomon encoder(204, 188);
                    for (int j = 0; j < 12; ++j) {
                        encoder.encode(data->enhancedPacketData[j], 188);
                    }
                    data->enhancedPacketWaiting = 192;
                }
            }
            written += length;
            dataBuffer += length;
        } else {
            int nbBytes = ops->read(args, dataBuffer, 3);
            header = (packetHeader*)dataBuffer;
            if (nbBytes == -1) {
                if (errno == EAGAIN) goto END_PACKET;
                perror("Packet file");
                return -1;
            } else if (nbBytes == 0) {
                if (ops->rewind(args) == -1) {
                    goto END_PACKET;
                }
                continue;
            } else if (nbBytes < 3) {
                etiLog.print(TcpLog::ERR,
                        "Error while reading file for packet header; "
                        "read %i out of 3 bytes\n", nbBytes);
                break;
            }

            length = header->packetLength * 24 + 24;
            if (written + length > size) {
                memcpy(data->packetData, header, 3);
                ops->read(args, &data->packetData[3], length - 3);
                data->packetLength = length;
                continue;
            }
            if (data->enhancedPacketData != NULL) {
                if (data->enhancedPacketLength + length > (12 * 188)) {
                    memcpy(data->packetData, header, 3);
                    ops->read(args, &data->packetData[3], length - 3);
                    data->packetLength = length;
                    continue;
                }
            }
            nbBytes = ops->read(args, dataBuffer + 3, length - 3);
            if (nbBytes == -1) {
                perror("Packet file");
                return -1;
            } else if (nbBytes == 0) {
                etiLog.print(TcpLog::NOTICE,
                        "Packet header read, but no data!\n");
                if (ops->rewind(args) == -1) {
                    goto END_PACKET;
                }
                continue;
            } else if (nbBytes < length - 3) {
                etiLog.print(TcpLog::ERR, "Error while reading packet file; "
                        "read %i out of %i bytes\n", nbBytes, length - 3);
                break;
            }
            if (data->enhancedPacketData != NULL) {
                indexCol = data->enhancedPacketLength / 12;
                indexRow = data->enhancedPacketLength % 12;     // TODO Check if always 0
                for (int j = 0; j < length; ++j) {
                    data->enhancedPacketData[indexRow][indexCol] = dataBuffer[j];
                    if (++indexRow == 12) {
                        indexRow = 0;
                        ++indexCol;
                    }
                }
                data->enhancedPacketLength += length;
                if (data->enhancedPacketLength >= (12 * 188)) {
                    if (data->enhancedPacketLength > (12 * 188)) {
                        etiLog.print(TcpLog::ERR,
                                "Error, too much enhanced packet data!\n");
                    }
                    ReedSolomon encoder(204, 188);
                    for (int j = 0; j < 12; ++j) {
                        encoder.encode(data->enhancedPacketData[j], 188);
                    }
                    data->enhancedPacketWaiting = 192;
                }
            }
            written += length;
            dataBuffer += length;
        }
    }
END_PACKET:
    if (ops->read == dabInputFifoRead) {
        dabInputFifoData* fifoData = (dabInputFifoData*)args;
        dabInputFifoStats* fifoStats = (dabInputFifoStats*)&fifoData->stats;
        fifoStats->frameRecords[fifoStats->frameCount].curSize = written;
        fifoStats->frameRecords[fifoStats->frameCount].maxSize = size;
        if (++fifoStats->frameCount == NB_RECORDS) {
            etiLog.print(TcpLog::INFO, "Packet subchannel usage: (%i)",
                    fifoStats->id);
            for (int i = 0; i < fifoStats->frameCount; ++i) {
            etiLog.print(TcpLog::INFO, " %i/%i",
                    fifoStats->frameRecords[i].curSize,
                    fifoStats->frameRecords[i].maxSize);
            }
            etiLog.print(TcpLog::INFO, "\n");
            fifoStats->frameCount = 0;
        }
    }
    while (written < size) {
        memset(dataBuffer, 0, 22);
        dataBuffer[22] = 0x60;
        dataBuffer[23] = 0x4b;
        dataBuffer += 24;
        written += 24;
    }
    return written;
}


#   endif
#endif
