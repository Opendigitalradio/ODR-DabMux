/*
   Copyright (C) 2009 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2016 Matthias P. Braendli
    http://www.opendigitalradio.org

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

#include <sstream>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#ifndef _WIN32
#   define O_BINARY 0
#endif

#include "input/File.h"
#include "mpeg.h"

namespace Inputs {

int FileBase::open(const std::string& name)
{
    m_fd = ::open(name.c_str(), O_RDONLY | O_BINARY);
    if (m_fd == -1) {
        std::stringstream ss;
        ss << "Could not open input file " << name << ": " <<
            strerror(errno);
        throw std::runtime_error(ss.str());
    }
    return 0;
}

int FileBase::close()
{
    if (m_fd != -1) {
        ::close(m_fd);
        m_fd = -1;
    }
    return 0;
}

int FileBase::rewind()
{
    return ::lseek(m_fd, 0, SEEK_SET);
}

int MPEGFile::readFrame(void* buffer, int size)
{
    int result;
    bool do_rewind = false;
READ_SUBCHANNEL:
    if (m_parity) {
        result = readData(m_fd, buffer, size, 2);
        m_parity = false;
        return 0;
    } else {
        result = readMpegHeader(m_fd, buffer, size);
        if (result > 0) {
            result = readMpegFrame(m_fd, buffer, size);
            if (result < 0 && getMpegFrequency(buffer) == 24000) {
                m_parity = true;
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
        if (do_rewind) {
            etiLog.log(error, "file rewinded and still empty "
                    "-> frame muted\n");
            goto MUTE_SUBCHANNEL;
        }
        else {
            etiLog.log(info, "reach end of file -> rewinding\n");
            rewind();
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
        }
        else {
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
                result = 0;
                break;
            case MPEG_ORIGINAL:
                result = 0;
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

int MPEGFile::setBitrate(int bitrate)
{
    if (bitrate == 0) {
        char buffer[4];

        if (readFrame(buffer, 4) == 0) {
            bitrate = getMpegBitrate(buffer);
        }
        else {
            bitrate = -1;
        }
        rewind();
    }
    return bitrate;
}


int DABPlusFile::readFrame(void* buffer, int size)
{
    uint8_t* dataOut = reinterpret_cast<uint8_t*>(buffer);

    ssize_t ret = read(m_fd, dataOut, size);

    if (ret == -1) {
        etiLog.log(alert, "ERROR: Can't read file\n");
        perror("");
        return -1;
    }

    if (ret < size) {
        ssize_t sizeOut = ret;
        etiLog.log(info, "reach end of file -> rewinding\n");
        if (rewind() == -1) {
            etiLog.log(alert, "ERROR: Can't rewind file\n");
            return -1;
        }

        ret = read(m_fd, dataOut + sizeOut, size - sizeOut);
        if (ret == -1) {
            etiLog.log(alert, "ERROR: Can't read file\n");
            perror("");
            return -1;
        }

        if (ret < size) {
            etiLog.log(alert, "ERROR: Not enough data in file\n");
            return -1;
        }
    }

    return size;
}

int DABPlusFile::setBitrate(int bitrate)
{
    if (bitrate <= 0) {
        etiLog.log(error, "Invalid bitrate (%i)\n", bitrate);
        return -1;
    }

    return bitrate;
}

};
