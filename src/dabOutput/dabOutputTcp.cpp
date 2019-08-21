/*
   Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Her Majesty the Queen in
   Right of Canada (Communications Research Center Canada)

   Copyright (C) 2018
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://www.opendigitalradio.org

   TCP output
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
#include <cstring>
#include <cstdio>
#include <signal.h>
#include <limits.h>
#include "dabOutput.h"
#include <unistd.h>
#include <sys/time.h>
#include <list>
#include <vector>
#include <atomic>
#include <thread>
#include "ThreadsafeQueue.h"

using namespace std;

using vec_u8 = std::vector<uint8_t>;

// In ETI one element would be an ETI frame of 6144 bytes.
// 250 frames correspond to 6 seconds. This is mostly here
// to ensure we do not accumulate data for faulty sockets, delay
// management has to be done on the receiver end.
const size_t MAX_QUEUED_ETI_FRAMES = 250;

static bool parse_uri(const char *uri, long *port, string& addr)
{
    char* const hostport = strdup(uri); // the uri is actually an tuple host:port

    char* address;
    address = strchr((char*)hostport, ':');
    if (address == NULL) {
        etiLog.log(error,
                "\"%s\" is an invalid format for tcp address: "
                "should be [address]:port - > aborting\n",
                hostport);
        goto tcp_open_fail;
    }

    // terminate string hostport after the host, and advance address to the port number
    *(address++) = 0;

    *port = strtol(address, (char **)NULL, 10);
    if ((*port == LONG_MIN) || (*port == LONG_MAX)) {
        etiLog.log(error,
                "can't convert port number in tcp address %s\n", address);
        goto tcp_open_fail;
    }
    if (*port == 0) {
        etiLog.log(error,
                "can't use port number 0 in tcp address\n");
        goto tcp_open_fail;
    }
    addr = hostport;
    free(hostport);
    return true;

tcp_open_fail:
    free(hostport);
    return false;
}

int DabOutputTcp::Open(const char* name)
{
    long port = 0;
    string address;
    bool success = parse_uri(name, &port, address);

    uri_ = name;

    if (success) {
        dispatcher_ = make_shared<Socket::TCPDataDispatcher>(MAX_QUEUED_ETI_FRAMES);
        dispatcher_->start(port, address);
    }
    else {
        throw runtime_error(string("Could not parse TCP output address ") + name);
    }
    return 0;
}


int DabOutputTcp::Write(void* buffer, int size)
{
    vec_u8 data(6144);
    uint8_t* buffer_u8 = (uint8_t*)buffer;

    std::copy(buffer_u8, buffer_u8 + size, data.begin());

    // Pad to 6144 bytes
    std::fill(data.begin() + size, data.end(), 0x55);

    dispatcher_->write(data);
    return size;
}


int DabOutputTcp::Close()
{
    return 0;
}
