/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009 Her Majesty the Queen in Right
   of Canada (Communications Research Center Canada)
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

#include "Dmb.h"

#include <string.h>


#ifdef _WIN32
#   define bzero(a, b) memset((a), 0, (b))
#endif // _WIN32


DmbStats::DmbStats()
{
    reset();
}

            
void DmbStats::reset()
{
    memset(this, 0, sizeof(*this));
}


Dmb::Dmb(bool reverse) :
interleaver(12, 17),
encoder(204, 188)
{
    buffer = NULL;
    bufferSize = 0;
    setReverse(reverse);
}


Dmb::~Dmb()
{
    if (buffer != NULL) {
        delete[] buffer;
    }
}


void Dmb::setReverse(bool state)
{
    reverse = state;
    reset();

    interleaver.setReverse(state);
    encoder.setReverse(state);
    stats.reset();
}


void Dmb::reset()
{
    bufferOffset = 0;
    bufferLength = 0;
    inputOffset = 0;
    outputOffset = 0;
//    padding = 0;
}


#include <stdio.h>
int Dmb::encode(
        void* dataIn,
        unsigned long sizeIn,
        void* dataOut,
        unsigned long sizeOut
        )
{
    int ret = 0;
    char* input = reinterpret_cast<char*>(dataIn);
    char* output = reinterpret_cast<char*>(dataOut);

    if (bufferSize < sizeIn + 204) {
        if (buffer != NULL) {
            delete[] buffer;
        }
        unsigned char* oldBuffer = buffer;
        bufferSize = sizeIn + 204;
        buffer = new unsigned char[bufferSize];
        memcpy(buffer, oldBuffer, bufferLength);
    }

    if (reverse) {
//        fprintf(stderr, "*** Decode ***\n");
//        fprintf(stderr, " sizeIn: %i\n", sizeIn);
//        fprintf(stderr, " inputOffset: %i\n", inputOffset);
//        fprintf(stderr, " bufferLength: %i\n", bufferLength);
//        fprintf(stderr, " bufferOffset: %i\n", bufferOffset);
//        fprintf(stderr, " outputOffset: %i\n", outputOffset);

        if (inputOffset == 0) {
            ++stats.dmbFrame;
            stats.rcvBytes += sizeIn;
        }

        memcpy(&buffer[bufferLength], &input[inputOffset],
                sizeIn - inputOffset);
        interleaver.encode(&buffer[bufferLength], sizeIn - inputOffset);
        bufferLength += sizeIn - inputOffset;
        inputOffset += sizeIn - inputOffset;
        while (bufferLength - bufferOffset >= 204) {
            if (buffer[bufferOffset] != 0x47) {
                int offset = sync(&buffer[bufferOffset],
                        bufferLength - bufferOffset);
                if (offset == 0) {
                    bufferLength = 0;
                    bufferOffset = 0;
                } else {
                    bufferOffset += offset;
                }
            } else {
                if (outputOffset + 188 <= sizeOut) { // If enough place
                    int error = encoder.encode(&buffer[bufferOffset], 204);
                    if (error != -1) {
//                    if (true) {
                        if (error > 0) {
                            stats.corBytes += error;
                        }
                        memcpy(&output[outputOffset], &buffer[bufferOffset],
                                188);
                        if (((buffer[bufferOffset + 1] & 0x1f) == 0x1f) &&
                                (buffer[bufferOffset + 2] == 0xff)) {
                            stats.nulBytes += 188;
                        } else {
                            ret += 188;
                            stats.sndBytes += 188;
                            outputOffset += 188;
                        }
                        bufferOffset += 204;
                        ++stats.mpgFrame;
                    } else {
//                        interleaver.reset();
//                        padding = 0;
//                        bufferLength = 0;
//                        inputOffset = 0;
//                        stats.errBytes += 188;
                        bufferOffset += 12;
                        stats.errBytes += 12;
                        ++stats.synFrame;
                        int offset = sync (&buffer[bufferOffset],
                                bufferLength - bufferOffset);
                        if (offset == 0) {
                            stats.errBytes += bufferLength - bufferOffset;
                            bufferOffset = 0;
                            bufferLength = 0;
                        } else {
                            stats.errBytes += offset;
                            bufferOffset += offset;
                        }
                    }
                } else {
                    outputOffset = 0;
                    goto ENCODE_END;
                }
            }
        }
        memmove(buffer, &buffer[bufferOffset], bufferLength - bufferOffset);
        bufferLength -= bufferOffset;
        bufferOffset = 0;
        if (ret == 0) {
            inputOffset = 0;
        } else {
            outputOffset = 0;
        }
//        fprintf(stderr, " ret: %i\n\n", ret);
    } else {
        if (sizeIn == 0) {
            bzero(&output[outputOffset], sizeOut - outputOffset);
            interleaver.encode(&output[outputOffset], sizeOut - outputOffset);
            outputOffset = 0;
            ++stats.dmbFrame;
            stats.sndBytes += sizeOut;
            goto ENCODE_END;
        }

        if (bufferLength == 204) {     // If buffer is not empty
            // If there's more data in buffer than place in output
            if (204 - bufferOffset > sizeOut - outputOffset) {
                memcpy(&output[outputOffset], &buffer[bufferOffset],
                        sizeOut - outputOffset);
                bufferOffset += sizeOut - outputOffset;
                outputOffset = 0;
                ++stats.dmbFrame;
                stats.sndBytes += sizeOut;
                goto ENCODE_END;
            }
            memcpy(&output[outputOffset], &buffer[bufferOffset],
                    204 - bufferOffset);
            outputOffset += 204 - bufferOffset;
            bufferOffset = 0;
            bufferLength = 0;
        }

        while (true) {
            // If there's not enought input data
            if (sizeIn - inputOffset < 188 - bufferLength) {
                memcpy(&buffer[bufferLength], &input[inputOffset],
                        sizeIn - inputOffset);
                bufferLength += sizeIn - inputOffset;
                ret = outputOffset;
                inputOffset = 0;
                goto ENCODE_END;
            }
            if (buffer[bufferLength] == 0x47) {
                ++stats.mpgFrame;
            }
//            fprintf(stderr, "++mpg: %i, sizeIn: %i, inputOffset: %i\n",
//            stats.mpgFrame, sizeIn, inputOffset);
            stats.rcvBytes += 188;

            memcpy(&buffer[bufferLength], &input[inputOffset],
                    188 - bufferLength);
            inputOffset += 188 - bufferLength;

            encoder.encode(buffer, 188);
            interleaver.encode(buffer, 204);
            bufferLength = 204;

            if (sizeOut - outputOffset < 204) {
                memcpy(&output[outputOffset], buffer, sizeOut - outputOffset);
                bufferOffset += sizeOut - outputOffset;
                outputOffset = 0;
                ++stats.dmbFrame;
                stats.sndBytes += sizeOut;
                goto ENCODE_END;
            }
            memcpy(&output[outputOffset], buffer, 204);
            outputOffset += 204;
            bufferLength = 0;
        }
    }

ENCODE_END:
    return ret;
}


int Dmb::sync(void* dataIn, unsigned long sizeIn)
{
    char* input = reinterpret_cast<char*>(dataIn);
    char* sync = input;
    unsigned long size = sizeIn;

    while (sync != NULL) {
        sync = (char*)memchr(sync, 0x47, size);
        if (sync != NULL) {
            int offset = sync - input;
            if (offset % 12 != 0) {
                offset = ((offset / 12) * 12) + 12;
                sync = &input[offset];
                size = sizeIn - offset;
            } else {
                return offset;
            }
        }
    }
    return 0;
}
