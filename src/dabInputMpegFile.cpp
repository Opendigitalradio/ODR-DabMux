/*
   Copyright (C) 2009 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)
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

#include "dabInputMpegFile.h"
#include "dabInputFile.h"
#include "mpeg.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>


#ifdef HAVE_FORMAT_MPEG
#   ifdef HAVE_INPUT_FILE


struct dabInputOperations dabInputMpegFileOperations = {
    dabInputFileInit,
    dabInputFileOpen,
    dabInputSetbuf,
    dabInputFileRead,
    nullptr,
    nullptr,
    dabInputMpegFileRead,
    dabInputMpegSetbitrate,
    dabInputFileClose,
    dabInputFileClean,
    dabInputFileRewind
};


int dabInputMpegFileRead(dabInputOperations* ops, void* args, void* buffer, int size)
{
    dabInputFileData* data = (dabInputFileData*)args;
    int result;
    bool rewind = false;
READ_SUBCHANNEL:
    if (data->parity) {
        result = readData(data->file, buffer, size, 2);
        data->parity = false;
        return 0;
    } else {
        result = readMpegHeader(data->file, buffer, size);
        if (result > 0) {
            result = readMpegFrame(data->file, buffer, size);
            if (result < 0 && getMpegFrequency(buffer) == 24000) {
                data->parity = true;
                result = size;
            }
        }
    }
    switch (result) {
    case MPEG_BUFFER_UNDERFLOW:
        etiLog.log(warn, "data underflow -> frame muted\n");
        goto MUTE_SUBCHANNEL;
    case MPEG_BUFFER_OVERFLOW:
        etiLog.log(warn, "bitrate too high -> frame muted\n");
        goto MUTE_SUBCHANNEL;
    case MPEG_FILE_EMPTY:
        if (rewind) {
            etiLog.log(error, "file rewinded and still empty "
                    "-> frame muted\n");
            goto MUTE_SUBCHANNEL;
        } else {
            rewind = true;
            etiLog.log(info, "reach end of file -> rewinding\n");
            lseek(data->file, 0, SEEK_SET);
            goto READ_SUBCHANNEL;
        }
    case MPEG_FILE_ERROR:
        etiLog.log(alert, "can't read file (%i) -> frame muted\n", errno);
        perror("");
        goto MUTE_SUBCHANNEL;
    case MPEG_SYNC_NOT_FOUND:
        etiLog.log(alert, "mpeg sync not found, maybe is not a valid file "
                "-> frame muted\n");
        goto MUTE_SUBCHANNEL;
    case MPEG_INVALID_FRAME:
        etiLog.log(alert, "file is not a valid mpeg file "
                "-> frame muted\n");
        goto MUTE_SUBCHANNEL;
    default:
        if (result < 0) {
            etiLog.log(alert,
                    "unknown error (code = %i) -> frame muted\n",
                    result);
MUTE_SUBCHANNEL:
            memset(buffer, 0, size);
        } else {
            if (result < size) {
                etiLog.log(warn, "bitrate too low from file "
                        "-> frame padded\n");
                memset((char*)buffer + result, 0, size - result);
            }
            result = checkDabMpegFrame(buffer);
            switch (result) {
            case MPEG_FREQUENCY:
                etiLog.log(error, "file has a frame with an invalid "
                        "frequency: %i, should be 48000 or 24000\n",
                        getMpegFrequency(buffer));
                break;
            case MPEG_PADDING:
                etiLog.log(warn,
                        "file has a frame with padding bit set\n");
                break;
            case MPEG_COPYRIGHT:
                etiLog.log(warn,
                        "file has a frame with copyright bit set\n");
                break;
            case MPEG_ORIGINAL:
                etiLog.log(warn,
                        "file has a frame with original bit set\n");
                break;
            case MPEG_EMPHASIS:
                etiLog.log(warn,
                        "file has a frame with emphasis bits set\n");
                break;
            default:
                if (result < 0) {
                    etiLog.log(alert, "mpeg file has an invalid DAB "
                            "mpeg frame (unknown reason: %i)\n", result);
                }
                break;
            }
        }
    }
    return result;
}


int dabInputMpegSetbitrate(dabInputOperations* ops, void* args, int bitrate)
{
    //dabInputFileData* data = (dabInputFileData*)args;
    if (bitrate == 0) {
        char buffer[4];

        if (ops->readFrame(ops, args, buffer, 4) == 0) {
            bitrate = getMpegBitrate(buffer);
        } else {
            bitrate = -1;
        }
        ops->rewind(args);
    }
    if (ops->setbuf(args, bitrate * 3) != 0) {
        bitrate = -1;
    }
    return bitrate;
}


#   endif
#endif
