/*
   Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Her Majesty the Queen in
   Right of Canada (Communications Research Center Canada)

   Copyright (C) 2013 Matthias P. Braendli
   http://mpb.li

   An object-oriented version of the output channels.
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

#ifndef __DAB_OUTPUT_H
#define __DAB_OUTPUT_H

#include "UdpSocket.h"
#include "TcpServer.h"
#include "Log.h"
#include <stdexcept>
#include <signal.h>
#ifdef _WIN32
#   include <io.h>
#   ifdef __MINGW32__
#       define FS_DECLARE_CFG_ARRAYS
#       include <winioctl.h>
#   endif
#   include <sdci.h>
#else
#   include <unistd.h>
#   include <sys/time.h>
#   ifndef O_BINARY
#       define O_BINARY 0
#   endif // O_BINARY
#endif
#ifdef HAVE_OUTPUT_ZEROMQ
#  include "zmq.hpp"
#endif

// Abstract base class for all outputs
class DabOutput
{
    public:
        virtual int Open(const char* name) = 0;
        int Open(std::string name)
        {
            return Open(name.c_str());
        }
        virtual int Write(void* buffer, int size) = 0;
        virtual int Close() = 0;

        virtual ~DabOutput() {};
};

// ----- used in File and Fifo outputs
enum EtiFileType {
    ETI_FILE_TYPE_NONE = 0,
    ETI_FILE_TYPE_RAW,
    ETI_FILE_TYPE_STREAMED,
    ETI_FILE_TYPE_FRAMED
};

// ---------- File output ------------
class DabOutputFile : public DabOutput
{
    public:
        DabOutputFile() {
            nbFrames_ = 0;
            file_ = -1;
            type_ = ETI_FILE_TYPE_FRAMED;
        }

        DabOutputFile(const DabOutputFile& other)
        {
            file_ = other.file_;
            nbFrames_ = other.nbFrames_;
            type_ = other.type_;
        }

        ~DabOutputFile() {}

        int Open(const char* filename);
        int Write(void* buffer, int size);
        int Close();

    protected:
        int file_;
        EtiFileType type_;
        unsigned long nbFrames_;
};

// ---------- FIFO output ------------
// only write is different for the FIFO output
class DabOutputFifo : public DabOutputFile
{
    public:
        DabOutputFifo() : DabOutputFile() {}
        ~DabOutputFifo() {}

        int Write(void* buffer, int size);
};

// -------------- RAW socket -----------
class DabOutputRaw : public DabOutput
{
    public:
        DabOutputRaw()
        {
#ifdef _WIN32
            socket_ = INVALID_HANDLE_VALUE;
#else
            socket_ = -1;
            isCyclades_ = false;
#endif
            buffer_ = new unsigned char[6144];
        }

        DabOutputRaw(const DabOutputRaw& other)
        {
            socket_ = other.socket_;
#ifndef _WIN32
            isCyclades_ = other.isCyclades_;
#endif
            buffer_ = other.buffer_;
        }

        ~DabOutputRaw() {
            delete[] buffer_;
        }

        int Open(const char* name);
        int Write(void* buffer, int size);
        int Close();
    private:
#ifdef _WIN32
        HANDLE socket_;
#else
        int socket_;
        bool isCyclades_;
#endif
        unsigned char* buffer_;
};

// -------------- UDP ------------------
class DabOutputUdp : public DabOutput
{
    public:
        DabOutputUdp() {
            UdpSocket::init();
            packet_ = new UdpPacket(6144);
            socket_ = new UdpSocket();
        }

        DabOutputUdp(const DabOutputUdp& other)
        {
            packet_ = other.packet_;
            socket_ = other.socket_;
        }

        ~DabOutputUdp() {
            delete socket_;
            delete packet_;
        }

        int Open(const char* name);
        int Write(void* buffer, int size);
        int Close() { return 0; }

    private:
        UdpSocket* socket_;
        UdpPacket* packet_;
};

// -------------- TCP ------------------
class DabOutputTcp : public DabOutput
{
    public:
        DabOutputTcp()
        {
            TcpSocket::init();
            server = new TcpServer();
            client = NULL;
        }

        DabOutputTcp(const DabOutputTcp& other)
        {
            server = other.server;
            client = other.client;
            thread_ = other.thread_;
        }

        ~DabOutputTcp() {

#ifdef _WIN32
            CloseHandle(this->thread_);
#endif

            delete this->server;
            delete this->client;
        }

        int Open(const char* name);
        int Write(void* buffer, int size);
        int Close();

        TcpServer* server;
        TcpSocket* client;
    private:
        pthread_t thread_;
};

// -------------- Simul ------------------
class DabOutputSimul : public DabOutput
{
    public:
        DabOutputSimul() {}

        DabOutputSimul(const DabOutputSimul& other)
        {
            startTime_ = other.startTime_;
        }

        ~DabOutputSimul() { }

        int Open(const char* name);
        int Write(void* buffer, int size);
        int Close() { return 0; }
    private:
#ifdef _WIN32
        DWORD startTime_;
#else
        timeval startTime_;
#endif
};

#if defined(HAVE_OUTPUT_ZEROMQ)
// -------------- ZeroMQ message queue ------------------
class DabOutputZMQ : public DabOutput
{
    public:
        DabOutputZMQ() :
            zmq_proto_(""), zmq_context_(1), zmq_pub_sock_(zmq_context_, ZMQ_PUB)
        {
            throw std::runtime_error("ERROR: No ZMQ protocol specified !");
        }

        DabOutputZMQ(std::string zmq_proto) :
            zmq_proto_(zmq_proto), zmq_context_(1), zmq_pub_sock_(zmq_context_, ZMQ_PUB)
        { }


        DabOutputZMQ(const DabOutputZMQ& other) :
            zmq_proto_(other.zmq_proto_), zmq_context_(1), zmq_pub_sock_(zmq_context_, ZMQ_PUB)
        {
            // cannot copy context
        }

        ~DabOutputZMQ()
        {
            zmq_pub_sock_.close();
        }

        int Open(const char* name);
        int Write(void* buffer, int size);
        int Close();
    private:
        std::string zmq_proto_;
        zmq::context_t zmq_context_; // handle for the zmq context
        zmq::socket_t zmq_pub_sock_; // handle for the zmq publisher socket
};

#endif

#endif

