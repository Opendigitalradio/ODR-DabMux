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
   This file is part of CRC-DabMux.

   It defines a ZeroMQ input for dabplus data.

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

#ifndef DAB_INPUT_ZMQ_H
#define DAB_INPUT_ZMQ_H

#ifdef HAVE_INPUT_ZEROMQ

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif
#include <zmq.h>
#include <list>
#include <string>
#include "dabInput.h"
#include "dabInputFifo.h"

// Number of frames to prebuffer
#define INPUT_ZMQ_PREBUFFERING 10

// Maximum frame_buffer size in number of elements
#define INPUT_ZMQ_MAX_BUFFER_SIZE 200


extern struct dabInputOperations dabInputZmqOperations;

struct dabInputZmqData {
    void* zmq_context;
    void* zmq_sock;
    std::list<char*> frame_buffer; //stores elements of type char[<framesize>]
    dabInputFifoStats stats;
    int prebuffering;
    std::string uri;
};


int dabInputZmqInit(void** args);
int dabInputZmqOpen(void* args, const char* inputUri);
int dabInputZmqReadFrame(dabInputOperations* ops, void* args, void* buffer, int size);
int dabInputZmqClose(void* args);
int dabInputZmqClean(void** args);

// Get new message from ZeroMQ
int dabInputZmqReadFromSocket(dabInputZmqData* input, int framesize);


#endif // HAVE_INPUT_ZMQ

#endif // DAB_INPUT_ZMQ_H
