/*
   Copyright (C) 2009 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2016
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

    return 0;
}

int Udp::readFrame(void* buffer, int size)
{
    uint8_t* data = reinterpret_cast<uint8_t*>(buffer);

    // Regardless of buffer contents, try receiving data.
    UdpPacket packet;
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
        std::copy(m_buffer.begin(), m_buffer.begin() + size, data);
    }
    else {
        memset(data, 0x0, size);
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

};
