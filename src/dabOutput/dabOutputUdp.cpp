/*
   Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Her Majesty the Queen in
   Right of Canada (Communications Research Center Canada)

   Copyright (C) 2016
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://www.opendigitalradio.org

   UDP output
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
#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#if defined(HAVE_OUTPUT_UDP)

#include <cstring>
#include <cstdlib>
#include <regex>
#include <string>
#include <cstdio>
#include <limits.h>
#include "dabOutput.h"
#include "Socket.h"

DabOutputUdp::DabOutputUdp() :
    socket_(),
    packet_(6144)
{ }

int DabOutputUdp::Open(const char* name)
{
    using namespace std;

    const string uri_without_proto(name);

    regex re_url("([^:]+):([0-9]+)(.*)");
    regex re_query("[?](?:src=([^&,]+))(?:[&,]ttl=([0-9]+))?");
    smatch what;
    if (regex_match(uri_without_proto, what, re_url,
                regex_constants::match_default)) {
        string address = what[1];

        string port_str = what[2];
        long port = std::strtol(port_str.c_str(), nullptr, 0);

        if ((port <= 0) || (port >= 65536)) {
            etiLog.level(error) <<
                "can't convert port number in UDP address " << uri_without_proto;
            return -1;
        }

        packet_.address.resolveUdpDestination(address, port);

        string query_params = what[3];
        smatch query_what;
        if (regex_match(query_params, query_what, re_query,
                    regex_constants::match_default)) {
            string src = query_what[1];

            try {
                socket_.setMulticastSource(src.c_str());

                string ttl_str = query_what[2];

                if (not ttl_str.empty()) {
                    long ttl = std::strtol(ttl_str.c_str(), nullptr, 0);
                    if ((ttl <= 0) || (ttl >= 255)) {
                        etiLog.level(error) << "Invalid TTL setting in " <<
                            uri_without_proto;
                        return -1;
                    }

                    socket_.setMulticastTTL(ttl);
                }
            }
            catch (const std::runtime_error& e) {
                etiLog.level(error) << "Failed to set UDP output settings" << e.what();
            }
        }
        else if (not query_params.empty()) {
            etiLog.level(error) << "UDP output: could not parse parameters " <<
                query_params;
            return -1;
        }
    }
    else {
        etiLog.level(error) << uri_without_proto <<
            " is an invalid format for UDP address: "
            "expected ADDRESS:PORT[?src=SOURCE&ttl=TTL]";
        return -1;
    }

    return 0;
}


int DabOutputUdp::Write(void* buffer, int size)
{
    const uint8_t *buf = reinterpret_cast<uint8_t*>(buffer);
    packet_.buffer.resize(0);
    std::copy(buf, buf + size, std::back_inserter(packet_.buffer));
    socket_.send(packet_);
    return 0;
}
#endif // defined(HAVE_OUTPUT_UDP)

