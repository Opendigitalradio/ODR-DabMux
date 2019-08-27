/*
   Copyright (C) 2009 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2019 Matthias P. Braendli
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

#include <errno.h>
#include <cstdlib>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#ifndef _WIN32
#   define O_BINARY 0
#endif
#include "input/File.h"
#include "mpeg.h"
#include "ReedSolomon.h"

using namespace std;

namespace Inputs {

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


void FileBase::open(const std::string& name)
{
    int flags = O_RDONLY | O_BINARY;
    if (m_nonblock) {
        flags |= O_NONBLOCK;
    }

    m_fd = ::open(name.c_str(), flags);
    if (m_fd == -1) {
        throw runtime_error("Could not open input file " + name + ": " +
            strerror(errno));
    }
}

size_t FileBase::readFrame(uint8_t *buffer, size_t size, uint32_t seconds, uint32_t tsta)
{
    // Will not be implemented, as there is no obvious way to carry timestamps
    // in files.
    memset(buffer, 0, size);
    return 0;
}

int FileBase::setBitrate(int bitrate)
{
    if (bitrate <= 0) {
        throw invalid_argument("Invalid bitrate " + to_string(bitrate));
    }

    return bitrate;
}


void FileBase::close()
{
    if (m_fd != -1) {
        ::close(m_fd);
        m_fd = -1;
    }
}

void FileBase::setNonblocking(bool nonblock)
{
    m_nonblock = nonblock;
}

int FileBase::rewind()
{
    return ::lseek(m_fd, 0, SEEK_SET);
}

ssize_t FileBase::readFromFile(uint8_t* buffer, size_t size)
{
    using namespace std;

    ssize_t ret = 0;
    if (m_nonblock) {
        if (size > m_nonblock_buffer.size()) {
            const size_t required_len = size - m_nonblock_buffer.size();
            vector<uint8_t> buf(required_len);
            ret = read(m_fd, buf.data(), required_len);

            /* If no process has the pipe open for writing, read() shall return 0
             * to indicate end-of-file. */
            if (ret == 0) {
                return 0;
            }

            /* If some process has the pipe open for writing and O_NONBLOCK is
             * set, read() shall return −1 and set errno to [EAGAIN]. */
            if (ret == -1 and errno == EAGAIN) {
                return 0;
            }
            else if (ret == -1) {
                etiLog.level(alert) << "ERROR: Can't read file " << strerror(errno);
                return -1;
            }

            // read() might read less data than requested
            buf.resize(ret);

            copy(buf.begin(), buf.end(), back_inserter(m_nonblock_buffer));
        }

        if (m_nonblock_buffer.size() >= size) {
            copy(m_nonblock_buffer.begin(), m_nonblock_buffer.begin() + size, buffer);

            vector<uint8_t> remaining_buf;
            copy(m_nonblock_buffer.begin() + size, m_nonblock_buffer.end(), back_inserter(remaining_buf));
            m_nonblock_buffer = move(remaining_buf);

            return size;
        }
        else {
            return 0;
        }
    }
    else {
        ret = read(m_fd, buffer, size);

        if (ret == -1) {
            etiLog.level(alert) << "ERROR: Can't read file " << strerror(errno);
            return -1;
        }

        if (ret < (ssize_t)size) {
            ssize_t sizeOut = ret;
            etiLog.log(info, "reach end of file -> rewinding");
            if (rewind() == -1) {
                etiLog.log(alert, "ERROR: Can't rewind file");
                return -1;
            }

            ret = read(m_fd, buffer + sizeOut, size - sizeOut);
            if (ret == -1) {
                etiLog.log(alert, "ERROR: Can't read file");
                perror("");
                return -1;
            }

            if (ret < (ssize_t)size) {
                etiLog.log(alert, "ERROR: Not enough data in file");
                return -1;
            }
        }
    }

    return size;
}

size_t MPEGFile::readFrame(uint8_t *buffer, size_t size)
{
    int result;
    bool do_rewind = false;
READ_SUBCHANNEL:
    if (m_parity) {
        result = readData(m_fd, buffer, size, 2);
        m_parity = false;
        return 0;
    }
    else {
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
            etiLog.log(warn, "data underflow -> frame muted");
            goto MUTE_SUBCHANNEL;
        case MPEG_BUFFER_OVERFLOW:
            etiLog.log(warn, "bitrate too high -> frame muted");
            goto MUTE_SUBCHANNEL;
        case MPEG_FILE_EMPTY:
            if (do_rewind) {
                etiLog.log(error, "file rewinded and still empty "
                        "-> frame muted");
                goto MUTE_SUBCHANNEL;
            }
            else {
                etiLog.log(info, "reach end of file -> rewinding");
                rewind();
                goto READ_SUBCHANNEL;
            }
        case MPEG_FILE_ERROR:
            etiLog.log(alert, "can't read file (%i) -> frame muted", errno);
            perror("");
            goto MUTE_SUBCHANNEL;
        case MPEG_SYNC_NOT_FOUND:
            etiLog.log(alert, "mpeg sync not found, maybe is not a valid file "
                    "-> frame muted");
            goto MUTE_SUBCHANNEL;
        case MPEG_INVALID_FRAME:
            etiLog.log(alert, "file is not a valid mpeg file "
                    "-> frame muted");
            goto MUTE_SUBCHANNEL;
        default:
            if (result < 0) {
                etiLog.log(alert,
                        "unknown error (code = %i) -> frame muted",
                        result);
MUTE_SUBCHANNEL:
                memset(buffer, 0, size);
            }
            else {
                if (result < (ssize_t)size) {
                    etiLog.log(warn, "bitrate too low from file "
                            "-> frame padded");
                    memset((char*)buffer + result, 0, size - result);
                }

                result = checkDabMpegFrame(buffer);
                switch (result) {
                    case MPEG_FREQUENCY:
                        etiLog.log(error, "file has a frame with an invalid "
                                "frequency: %i, should be 48000 or 24000",
                                getMpegFrequency(buffer));
                        break;
                    case MPEG_PADDING:
                        etiLog.log(warn, "file has a frame with padding bit setn");
                        break;
                    case MPEG_COPYRIGHT:
                        result = 0;
                        break;
                    case MPEG_ORIGINAL:
                        result = 0;
                        break;
                    case MPEG_EMPHASIS:
                        etiLog.log(warn, "file has a frame with emphasis bits set");
                        break;
                    default:
                        if (result < 0) {
                            etiLog.log(alert, "mpeg file has an invalid DAB "
                                    "mpeg frame (unknown reason: %i)", result);
                        }
                        break;
                }
            }
    }

    // TODO this is probably wrong, because it should return
    // the number of bytes written.
    return result;
}

int MPEGFile::setBitrate(int bitrate)
{
    if (bitrate < 0) {
        throw invalid_argument("Invalid bitrate " + to_string(bitrate));
    }
    else if (bitrate == 0) {
        uint8_t buffer[4];

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

size_t RawFile::readFrame(uint8_t *buffer, size_t size)
{
    return readFromFile(buffer, size);
}

PacketFile::PacketFile(bool enhancedPacketMode)
{
    m_enhancedPacketEnabled = enhancedPacketMode;
}

size_t PacketFile::readFrame(uint8_t *buffer, size_t size)
{
    size_t written = 0;
    int length;
    packetHeader* header;
    int indexRow;
    int indexCol;

    while (written < size) {
        if (m_enhancedPacketWaiting > 0) {
            *buffer = 192 - m_enhancedPacketWaiting;
            *buffer /= 22;
            *buffer <<= 2;
            *(buffer++) |= 0x03;
            *(buffer++) = 0xfe;
            indexCol = 188;
            indexCol += (192 - m_enhancedPacketWaiting) / 12;
            indexRow = 0;
            indexRow += (192 - m_enhancedPacketWaiting) % 12;
            for (int j = 0; j < 22; ++j) {
                if (m_enhancedPacketWaiting == 0) {
                    *(buffer++) = 0;
                }
                else {
                    *(buffer++) = m_enhancedPacketData[indexRow][indexCol];
                    if (++indexRow == 12) {
                        indexRow = 0;
                        ++indexCol;
                    }
                    --m_enhancedPacketWaiting;
                }
            }
            written += 24;
            if (m_enhancedPacketWaiting == 0) {
                m_enhancedPacketLength = 0;
            }
        }
        else if (m_packetLength != 0) {
            header = (packetHeader*)(&m_packetData[0]);
            if (written + m_packetLength > (unsigned)size) {
                memset(buffer, 0, 22);
                buffer[22] = 0x60;
                buffer[23] = 0x4b;
                length = 24;
            }
            else if (m_enhancedPacketEnabled) {
                if (m_enhancedPacketLength + m_packetLength > (12 * 188)) {
                    memset(buffer, 0, 22);
                    buffer[22] = 0x60;
                    buffer[23] = 0x4b;
                    length = 24;
                }
                else {
                    copy(m_packetData.begin(),
                            m_packetData.begin() + m_packetLength,
                            buffer);
                    length = m_packetLength;
                    m_packetLength = 0;
                }
            }
            else {
                copy(m_packetData.begin(),
                        m_packetData.begin() + m_packetLength,
                        buffer);
                length = m_packetLength;
                m_packetLength = 0;
            }

            if (m_enhancedPacketEnabled) {
                indexCol = m_enhancedPacketLength / 12;
                indexRow = m_enhancedPacketLength % 12;     // TODO Check if always 0
                for (int j = 0; j < length; ++j) {
                    m_enhancedPacketData[indexRow][indexCol] = buffer[j];
                    if (++indexRow == 12) {
                        indexRow = 0;
                        ++indexCol;
                    }
                }
                m_enhancedPacketLength += length;
                if (m_enhancedPacketLength >= (12 * 188)) {
                    m_enhancedPacketLength = (12 * 188);
                    ReedSolomon encoder(204, 188);
                    for (int j = 0; j < 12; ++j) {
                        encoder.encode(&m_enhancedPacketData[j][0], 188);
                    }
                    m_enhancedPacketWaiting = 192;
                }
            }
            written += length;
            buffer += length;
        }
        else {
            int nbBytes = readFromFile(buffer, 3);
            header = (packetHeader*)buffer;
            if (nbBytes == -1) {
                if (errno == EAGAIN) goto END_PACKET;
                perror("Packet file");
                return -1;
            }
            else if (nbBytes == 0) {
                if (rewind() == -1) {
                    goto END_PACKET;
                }
                continue;
            }
            else if (nbBytes < 3) {
                etiLog.log(error,
                        "Error while reading file for packet header; "
                        "read %i out of 3 bytes", nbBytes);
                break;
            }

            length = header->packetLength * 24 + 24;
            if (written + length > size) {
                memcpy(&m_packetData[0], header, 3);
                readFromFile(&m_packetData[3], length - 3);
                m_packetLength = length;
                continue;
            }

            if (m_enhancedPacketEnabled) {
                if (m_enhancedPacketLength + length > (12 * 188)) {
                    memcpy(&m_packetData[0], header, 3);
                    readFromFile(&m_packetData[3], length - 3);
                    m_packetLength = length;
                    continue;
                }
            }

            nbBytes = readFromFile(buffer + 3, length - 3);
            if (nbBytes == -1) {
                perror("Packet file");
                return -1;
            }
            else if (nbBytes == 0) {
                etiLog.log(info,
                        "Packet header read, but no data!");
                if (rewind() == -1) {
                    goto END_PACKET;
                }
                continue;
            }
            else if (nbBytes < length - 3) {
                etiLog.log(error, "Error while reading packet file; "
                        "read %i out of %i bytes", nbBytes, length - 3);
                break;
            }

            if (m_enhancedPacketEnabled) {
                indexCol = m_enhancedPacketLength / 12;
                indexRow = m_enhancedPacketLength % 12;     // TODO Check if always 0
                for (int j = 0; j < length; ++j) {
                    m_enhancedPacketData[indexRow][indexCol] = buffer[j];
                    if (++indexRow == 12) {
                        indexRow = 0;
                        ++indexCol;
                    }
                }
                m_enhancedPacketLength += length;
                if (m_enhancedPacketLength >= (12 * 188)) {
                    if (m_enhancedPacketLength > (12 * 188)) {
                        etiLog.log(error,
                                "Error, too much enhanced packet data!");
                    }
                    ReedSolomon encoder(204, 188);
                    for (int j = 0; j < 12; ++j) {
                        encoder.encode(&m_enhancedPacketData[j][0], 188);
                    }
                    m_enhancedPacketWaiting = 192;
                }
            }
            written += length;
            buffer += length;
        }
    }
END_PACKET:
    while (written < size) {
        memset(buffer, 0, 22);
        buffer[22] = 0x60;
        buffer[23] = 0x4b;
        buffer += 24;
        written += 24;
    }
    return written;
}

};
