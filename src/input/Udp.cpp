/*
   Copyright (C) 2009 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2017
   Matthias P. Braendli, matthias.braendli@mpb.li

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

#include "input/Udp.h"

#include <stdexcept>
#include <sstream>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <errno.h>
#include "utils.h"

using namespace std;

namespace Inputs {

int Udp::open(const std::string& name)
{
    // Skip the udp:// part if it is present
    const string endpoint = (name.substr(0, 6) == "udp://") ?
        name.substr(6) : name;

    // The endpoint should be address:port
    const auto colon_pos = endpoint.find_first_of(":");
    if (colon_pos == string::npos) {
        stringstream ss;
        ss << "'" << name <<
                " is an invalid format for udp address: "
                "expected [udp://]address:port";
        throw invalid_argument(ss.str());
    }

    m_name = name;

    openUdpSocket(endpoint);

    return 0;
}

void Udp::openUdpSocket(const std::string& endpoint)
{
    const auto colon_pos = endpoint.find_first_of(":");
    const auto address = endpoint.substr(0, colon_pos);
    const auto port_str = endpoint.substr(colon_pos + 1);

    const long port = strtol(port_str.c_str(), nullptr, 10);

    if ((port == LONG_MIN or port == LONG_MAX) and errno == ERANGE) {
        throw out_of_range("udp input: port out of range");
    }
    else if (port == 0 and errno != 0) {
        stringstream ss;
        ss << "udp input port parse error: " << strerror(errno);
        throw invalid_argument(ss.str());
    }

    if (port == 0) {
        throw out_of_range("can't use port number 0 in udp address");
    }

    if (m_sock.reinit(port, address) == -1) {
        stringstream ss;
        ss << "Could not init UDP socket: " << inetErrMsg;
        throw runtime_error(ss.str());
    }

    if (m_sock.setBlocking(false) == -1) {
        stringstream ss;
        ss << "Could not set non-blocking UDP socket: " << inetErrMsg;
        throw runtime_error(ss.str());
    }

    etiLog.level(info) << "Opened UDP port " << address << ":" << port;
}

int Udp::readFrame(uint8_t* buffer, size_t size)
{
    // Regardless of buffer contents, try receiving data.
    UdpPacket packet(32768);
    int ret = m_sock.receive(packet);

    if (ret == -1) {
        stringstream ss;
        ss << "Could not read from UDP socket: " << inetErrMsg;
        throw runtime_error(ss.str());
    }

    std::copy(packet.getData(), packet.getData() + packet.getSize(),
            back_inserter(m_buffer));

    // Take data from the buffer if it contains enough data,
    // in any case write the buffer
    if (m_buffer.size() >= (size_t)size) {
        std::copy(m_buffer.begin(), m_buffer.begin() + size, buffer);
    }
    else {
        memset(buffer, 0x0, size);
    }

    return size;
}

int Udp::setBitrate(int bitrate)
{
    if (bitrate <= 0) {
        etiLog.log(error, "Invalid bitrate (%i)\n", bitrate);
        return -1;
    }

    return bitrate;
}

int Udp::close()
{
    return m_sock.close();
}


// ETSI EN 300 797 V1.2.1 ch 8.2.1.2
#define STI_SYNC_LEN 3
static const uint8_t STI_FSync0[STI_SYNC_LEN] = { 0x1F, 0x90, 0xCA };
static const uint8_t STI_FSync1[STI_SYNC_LEN] = { 0xE0, 0x6F, 0x35 };

static const size_t RTP_HEADER_LEN = 12;

static bool stiSyncValid(const uint8_t *buf)
{
    return  (memcmp(buf, STI_FSync0, sizeof(STI_FSync0)) == 0) or
            (memcmp(buf, STI_FSync1, sizeof(STI_FSync1)) == 0);
}

static bool rtpHeaderValid(const uint8_t *buf)
{
    uint32_t version = (buf[0] & 0xC0) >> 6;
    uint32_t payloadType = (buf[1] & 0x7F);
    return (version == 2 and payloadType == 34);
}

static uint16_t unpack2(const uint8_t *buf)
{
    return (((uint16_t)buf[0]) << 8) | buf[1];
}

int Sti_d_Rtp::open(const std::string& name)
{
    // Skip the sti-rtp:// part if it is present
    const string endpoint = (name.substr(0, 10) == "sti-rtp://") ?
        name.substr(10) : name;

    // The endpoint should be address:port
    const auto colon_pos = endpoint.find_first_of(":");
    if (colon_pos == string::npos) {
        stringstream ss;
        ss << "'" << name <<
                " is an invalid format for sti-rtp address: "
                "expected [sti-rtp://]address:port";
        throw invalid_argument(ss.str());
    }

    m_name = name;

    openUdpSocket(endpoint);

    return 0;
}

void Sti_d_Rtp::receive_packet()
{
    UdpPacket packet(32768);
    int ret = m_sock.receive(packet);

    if (ret == -1) {
        stringstream ss;
        ss << "Could not read from UDP socket: " << inetErrMsg;
        throw runtime_error(ss.str());
    }

    if (packet.getSize() == 0) {
        // No packet was received
        return;
    }

    const size_t STI_FC_LEN = 8;

    if (packet.getSize() < RTP_HEADER_LEN + STI_SYNC_LEN + STI_FC_LEN) {
        etiLog.level(info) << "Received too small RTP packet for " <<
            m_name;
        return;
    }

    if (not rtpHeaderValid(packet.getData())) {
        etiLog.level(info) << "Received invalid RTP header for " <<
            m_name;
        return;
    }

    //  STI(PI, X)
    size_t index = RTP_HEADER_LEN;
    const uint8_t *buf = packet.getData();

    //   SYNC
    index++; // Advance over STAT

    //    FSYNC
    if (not stiSyncValid(buf + index)) {
        etiLog.level(info) << "Received invalid STI-D header for " <<
            m_name;
        return;
    }
    index += 3;

    //  TFH
    //   DFS
    uint16_t DFS = unpack2(buf+index);
    index += 2;
    if (DFS == 0) {
        etiLog.level(info) << "Received STI data with DFS=0 for " <<
            m_name;
        return;
    }
    if (packet.getSize() < index + DFS) {
        etiLog.level(info) << "Received STI too small for given DFS for " <<
            m_name;
        return;
    }

    //   CFS
    uint32_t CFS = unpack2(buf+index);
    index += 2;
    if (CFS != 0) {
        etiLog.level(info) << "Ignoring STI control data for " <<
            m_name;
    }

    //   SPID
    index += 2;
    //   rfa DL
    index += 2;
    //   rfa
    index += 1;

    //  DFCT
    uint8_t  DFCTL = buf[index];
    index += 1;
    uint8_t  DFCTH = buf[index] >> 3;
    uint16_t NST   = unpack2(buf+index) & 0x7FF; // 11 bits
    index += 2;

    if (packet.getSize() < index + 4*NST) {
        etiLog.level(info) << "Received STI too small to contain NST for " <<
            m_name << " packet: " << packet.getSize() << " need " <<
            index + 4*NST;
        return;
    }

    if (NST == 0) {
        etiLog.level(info) << "Received STI with NST=0 for " <<
            m_name;
        return;
    }
    else {
        // Take the first stream even if NST > 1
        uint32_t STL = unpack2(buf+index) & 0x1FFF; // 13 bits
        uint32_t CRCSTF = buf[index+3] & 0x80 >> 7; // 7th bit
        index += NST*4+4;

        const size_t dataSize = STL - 2*CRCSTF;
        const size_t frameNumber = DFCTH*250 + DFCTL;
        // TODO must align framenumber with ETI

        vec_u8 data(dataSize);
        copy(buf+index, buf+index+dataSize, data.begin());

        // TODO reordering
        m_queue.push_back(data);
    }

    if (NST > 1) {
        etiLog.level(info) << "Ignoring STI supernumerary STC streams for " <<
            m_name;
    }
}

int Sti_d_Rtp::readFrame(uint8_t* buffer, size_t size)
{
    // Make sure we fill faster than we consume in case there
    // are pending packets.
    receive_packet();
    receive_packet();

    if (m_queue.empty()) {
        memset(buffer, 0x0, size);
    }
    else if (m_queue.front().size() != size) {
        etiLog.level(warn) << "Invalid input data size for STI " << m_name <<
            " : RX " << m_queue.front().size() << " expected " << size;
        memset(buffer, 0x0, size);
        m_queue.pop_front();
    }
    else {
        copy(m_queue.front().begin(), m_queue.front().end(), buffer);
        m_queue.pop_front();
    }

    return 0;
}

}
