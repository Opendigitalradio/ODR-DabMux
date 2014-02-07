/*
   Copyright (C) 2009 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2013 Matthias P. Braendli
   http://mpb.li

   ZeroMQ input. see www.zeromq.org for more info

   From the ZeroMQ manpage 'zmq':

       The 0MQ lightweight messaging kernel is a library which extends the standard
       socket interfaces with features traditionally provided by specialised
       messaging middleware products. 0MQ sockets provide an abstraction of
       asynchronous message queues, multiple messaging patterns, message filtering
       (subscriptions), seamless access to multiple transport protocols and more.
   */
/*
   This file is part of ODR-DabMux.

   It defines a ZeroMQ input for dabplus data.

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

#ifndef DAB_INPUT_ZMQ_H
#define DAB_INPUT_ZMQ_H

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#ifdef HAVE_INPUT_ZEROMQ

#include <zmq.hpp>
#include <list>
#include <string>
#include "dabInput.h"
#include "StatsServer.h"

/* The frame_buffer contains DAB logical frames as defined in
 * TS 102 563, section 6.
 * Five elements of this buffer make one AAC superframe (120ms audio)
 */

// Number of elements to prebuffer before starting the pipeline
#define INPUT_ZMQ_PREBUFFERING (5*4) // 480ms

// Maximum frame_buffer size in number of elements
#define INPUT_ZMQ_MAX_BUFFER_SIZE (5*8) // 960ms


class DabInputZmq : public DabInputBase, public RemoteControllable {
    public:
        DabInputZmq(const std::string name)
            : RemoteControllable(name),
            m_name(name), m_zmq_context(1),
            m_zmq_sock(m_zmq_context, ZMQ_SUB),
            m_prebuffering(INPUT_ZMQ_PREBUFFERING),
            m_bitrate(0) {
                RC_ADD_PARAMETER(buffer,
                        "Size of the input buffer [aac superframes]");
            }

        virtual int open(const std::string inputUri);
        virtual int readFrame(void* buffer, int size);
        virtual int setBitrate(int bitrate);
        virtual int close();

        /* Remote control */
        virtual void set_parameter(string parameter, string value);

        /* Getting a parameter always returns a string. */
        virtual string get_parameter(string parameter);

    private:
        int readFromSocket(int framesize);

        std::string m_name;
        zmq::context_t m_zmq_context;
        zmq::socket_t m_zmq_sock; // handle for the zmq socket
        int m_prebuffering;
        std::list<char*> m_frame_buffer; //stores elements of type char[<framesize>]
        int m_bitrate;
};

#endif // HAVE_INPUT_ZMQ

#endif // DAB_INPUT_ZMQ_H
