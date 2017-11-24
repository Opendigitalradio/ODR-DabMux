/*
   Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Her Majesty the Queen in
   Right of Canada (Communications Research Center Canada)

   Copyright (C) 2013, 2014 Matthias P. Braendli
   http://mpb.li

    http://opendigitalradio.org

   ZeroMQ output. see www.zeromq.org for more info

   From the ZeroMQ manpage 'zmq':

       The 0MQ lightweight messaging kernel is a library which extends the standard
       socket interfaces with features traditionally provided by specialised
       messaging middleware products. 0MQ sockets provide an abstraction of
       asynchronous message queues, multiple messaging patterns, message filtering
       (subscriptions), seamless access to multiple transport protocols and more.

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

#if defined(HAVE_OUTPUT_ZEROMQ)

#include <stdexcept>
#include <cstdio>
#include <cstring>
#include <signal.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include "zmq.hpp"
#include "dabOutput.h"

using namespace std;

int DabOutputZMQ::Open(const char* endpoint)
    // the format for endpoint shall be as defined in the zmq_bind manpage
    // but without specifying the protocol. The protocol has been given in
    // the constructor
{
    // bind to uri
    string proto_endpoint = zmq_proto_ + "://" + std::string(endpoint);
    std::cerr << "ZMQ socket " << proto_endpoint << std::endl;
    zmq_pub_sock_.bind(proto_endpoint.c_str());
    endpoint_ = endpoint;

    return 0;
}


int DabOutputZMQ::Write(void* buffer, int size)
{
    int offset = 0;

    // Increment the offset by the accumulated frame offsets
    for (int i = 0; i < zmq_message_ix; i++) {
        offset += zmq_message.buflen[i];
    }

    if (offset + size > NUM_FRAMES_PER_ZMQ_MESSAGE*6144) {
        throw std::runtime_error("FAULT: invalid ETI frame size!");
    }

    // Append the new frame to our message
    memcpy(zmq_message.buf + offset, buffer, size);
    zmq_message.buflen[zmq_message_ix] = size;
    zmq_message_ix++;

    // As soon as we have NUM_FRAMES_PER_ZMQ_MESSAGE frames, we transmit
    if (zmq_message_ix == NUM_FRAMES_PER_ZMQ_MESSAGE) {

        // Size of the header:
        size_t full_length = ZMQ_DAB_MESSAGE_HEAD_LENGTH;

        for (int i = 0; i < NUM_FRAMES_PER_ZMQ_MESSAGE; i++) {
            full_length += zmq_message.buflen[i];
        }

        vector<uint8_t> msg(full_length);
        memcpy(msg.data(), (uint8_t*)&zmq_message, full_length);

        // metadata gets appended at the end
        for (const auto& md : meta_) {
            vector<uint8_t> md_data(md->getLength());
            md->write(md_data.data());

            copy(md_data.begin(), md_data.end(), back_inserter(msg));
        }

        const int flags = 0;
        zmq_send(zmq_pub_sock_, msg.data(), msg.size(), flags);

        meta_.clear();
        zmq_message_ix = 0;
        for (int i = 0; i < NUM_FRAMES_PER_ZMQ_MESSAGE; i++) {
            zmq_message.buflen[i] = -1;
        }
    }

    return size;
}


int DabOutputZMQ::Close()
{
    return zmq_close(zmq_pub_sock_);
}

void DabOutputZMQ::setMetadata(std::shared_ptr<OutputMetadata> &md)
{
    if (m_allow_metadata) {
        meta_.push_back(md);
    }
}

#endif

