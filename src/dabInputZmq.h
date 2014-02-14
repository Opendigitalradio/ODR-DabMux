/*
   Copyright (C) 2009 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2013, 2014 Matthias P. Braendli
   http://mpb.li

   ZeroMQ input. see www.zeromq.org for more info

   For the AAC+ input, each zeromq message must contain one superframe.

   For the MPEG input, each zeromq message must contain one frame.

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

#include <list>
#include <string>
#include "zmq.hpp"
#include "dabInput.h"
#include "StatsServer.h"

/* The frame_buffer contains DAB logical frames as defined in
 * TS 102 563, section 6.
 * Five elements of this buffer make one AAC superframe (120ms audio)
 */

// Number of elements to prebuffer before starting the pipeline
#define INPUT_ZMQ_PREBUFFERING (5*4) // 480ms

// Default frame_buffer size in number of elements
#define INPUT_ZMQ_DEF_BUFFER_SIZE (5*8) // 960ms

// Minimum frame_buffer size in number of elements
// This is one AAC superframe, but you probably don't want to
// go that low anyway.
#define INPUT_ZMQ_MIN_BUFFER_SIZE (5*1) // 120ms

// Maximum frame_buffer size in number of elements
// One minute is clearly way over what everybody would
// want.
#define INPUT_ZMQ_MAX_BUFFER_SIZE (5*500) // 60s

class DabInputZmqBase : public DabInputBase, public RemoteControllable {
    public:
        DabInputZmqBase(const std::string name)
            : RemoteControllable(name),
            m_zmq_context(1),
            m_zmq_sock(m_zmq_context, ZMQ_SUB),
            m_bitrate(0), m_prebuffering(INPUT_ZMQ_PREBUFFERING),
            m_enable_input(true),
            m_frame_buffer_limit(INPUT_ZMQ_DEF_BUFFER_SIZE) { }

        virtual int open(const std::string inputUri);
        virtual int readFrame(void* buffer, int size);
        virtual int setBitrate(int bitrate);
        virtual int close();

        /* Remote control */
        virtual void set_parameter(const string& parameter,
               const string& value);

        /* Getting a parameter always returns a string. */
        virtual const string get_parameter(const string& parameter) const;

    protected:
        virtual int readFromSocket(int framesize) = 0;

        zmq::context_t m_zmq_context;
        zmq::socket_t m_zmq_sock; // handle for the zmq socket
        int m_bitrate;
        int m_prebuffering;

        /* set this to zero to empty the input buffer */
        bool m_enable_input;

        size_t m_frame_buffer_limit;
        std::list<char*> m_frame_buffer; //stores elements of type char[<framesize>]
};

class DabInputZmqMPEG : public DabInputZmqBase {
    public:
        DabInputZmqMPEG(const std::string name)
            : DabInputZmqBase(name) {
                RC_ADD_PARAMETER(buffer,
                        "Size of the input buffer [mpeg frames]");
                RC_ADD_PARAMETER(enable,
                        "If the input is enabled. Set to zero to empty the buffer.");
            }

    private:
        virtual int readFromSocket(int framesize);
};

class DabInputZmqAAC : public DabInputZmqBase {
    public:
        DabInputZmqAAC(const std::string name)
            : DabInputZmqBase(name) {
                RC_ADD_PARAMETER(buffer,
                        "Size of the input buffer [aac superframes]");
                RC_ADD_PARAMETER(enable,
                        "If the input is enabled. Set to zero to empty the buffer.");
            }

    private:
        virtual int readFromSocket(int framesize);
};

#endif // HAVE_INPUT_ZMQ

#endif // DAB_INPUT_ZMQ_H
