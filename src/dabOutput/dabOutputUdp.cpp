/*
   Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Her Majesty the Queen in
   Right of Canada (Communications Research Center Canada)

   Copyright (C) 2013 Matthias P. Braendli
   http://mpb.li

   UDP output
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

#include <cstring>
#include <cstdio>
#include <limits.h>
#include "dabOutput.h"
#include "UdpSocket.h"

#ifdef _WIN32
#   include <fscfg.h>
#   include <sdci.h>
#else
#   include <netinet/in.h>
#   include <sys/types.h>
#   include <sys/socket.h>
#   include <sys/ioctl.h>
#   include <linux/if_packet.h>
#   include <linux/netdevice.h>
#   include <net/if_arp.h>
#endif

int DabOutputUdp::Open(const char* name)
{
    // we are going to modify it
    char* hostport = strdup(name); // the name is actually an tuple host:port

    char* address;
    long port;
    address = strchr(hostport, ':');
    if (address == NULL) {
        etiLog.printHeader(TcpLog::ERR,
                "\"%s\" is an invalid format for UDP address: "
                "should be [address]:port - > aborting\n",
                hostport);
        goto udp_open_fail;
    }

    // terminate string hostport after the host, and advance address to the port number
    *(address++) = 0;

    port = strtol(address, (char **)NULL, 10);
    if ((port == LONG_MIN) || (port == LONG_MAX)) {
        etiLog.printHeader(TcpLog::ERR,
                "can't convert port number in UDP address %s\n", address);
        goto udp_open_fail;
    }
    if (port == 0) {
        etiLog.printHeader(TcpLog::ERR,
                "can't use port number 0 in UDP address\n");
        goto udp_open_fail;
    }
    address = hostport;
    if (strlen(address) > 0) {
        if (this->packet_->getAddress().setAddress(address) == -1) {
            etiLog.printHeader(TcpLog::ERR, "can't set address %s (%s: %s) "
                    "-> aborting\n", address, inetErrDesc, inetErrMsg);
            goto udp_open_fail;
        }
    }
    this->packet_->getAddress().setPort(port);

    if (this->socket_->create() == -1) {
        etiLog.printHeader(TcpLog::ERR, "can't create UDP socket (%s: %s) "
                "-> aborting\n)", inetErrDesc, inetErrMsg);
        goto udp_open_fail;
    }

    //sprintf(hostport, "%s:%i", this->packet_->getAddress().getHostAddress(),
    //        this->packet_->getAddress().getPort());
    return 0;

udp_open_fail:
    // strdup forces us to free
    free(hostport);
    return -1;
}


int DabOutputUdp::Write(void* buffer, int size)
{
    this->packet_->setLength(0);
    this->packet_->addData(buffer, size);
    return this->socket_->send(*this->packet_);
}
