/*
   Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Her Majesty the Queen in
   Right of Canada (Communications Research Center Canada)

   Copyright (C) 2013, 2014 Matthias P. Braendli
   http://mpb.li

    http://opendigitalradio.org

   An object-oriented version of the output channels.
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

#ifndef __DAB_OUTPUT_H
#define __DAB_OUTPUT_H

#include "UdpSocket.h"
#include "TcpServer.h"
#include "Log.h"
#include "string.h"
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

// Configuration for EDI output
struct edi_configuration_t {
    unsigned chunk_len; // RSk, data length of each chunk
    unsigned fec;       // number of fragments that can be recovered
    bool enabled;
    unsigned int source_port;
    bool dump;
    bool verbose;
    bool enable_pft;
    std::string dest_addr;
    unsigned int dest_port;
    unsigned int tagpacket_alignment;
};


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

        virtual ~DabOutput() {}

        virtual std::string get_info() = 0;
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

        std::string get_info() {
            return "file://" + filename_;
        }

    protected:
        std::string filename_;
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

        std::string get_info() {
            return "fifo://" + filename_;
        }

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
            buffer_ = new unsigned char[6144];
            memcpy(buffer_, other.buffer_, 6144);
        }

        ~DabOutputRaw() {
            delete[] buffer_;
        }

        const DabOutputRaw operator=(const DabOutputRaw& other);

        int Open(const char* name);
        int Write(void* buffer, int size);
        int Close();

        std::string get_info() {
            return "raw://" + filename_;
        }
    private:
        std::string filename_;
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

        // make sure we don't copy this output around
        // the UdpPacket and UdpSocket do not support
        // copying either
        DabOutputUdp(const DabOutputUdp& other);
        DabOutputUdp operator=(const DabOutputUdp& other);

        ~DabOutputUdp() {
            delete socket_;
            delete packet_;
        }

        int Open(const char* name);
        int Write(void* buffer, int size);
        int Close() { return 0; }

        std::string get_info() {
            return "udp://" + uri_;
        }
    private:
        std::string uri_;
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

        DabOutputTcp(const DabOutputTcp& other);
        DabOutputTcp operator=(const DabOutputTcp& other);

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

        std::string get_info() {
            return "tcp://" + uri_;
        }

        TcpServer* server;
        TcpSocket* client;
    private:
        std::string uri_;
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

        std::string get_info() {
            return "simul://" + name_;
        }
    private:
        std::string name_;
#ifdef _WIN32
        DWORD startTime_;
#else
        struct timespec startTime_;
#endif
};

#if defined(HAVE_OUTPUT_ZEROMQ)

#define NUM_FRAMES_PER_ZMQ_MESSAGE 4
/* A concatenation of four ETI frames,
 * whose maximal size is 6144.
 *
 * If we transmit four frames in one zmq message,
 * we do not risk breaking ETI vs. transmission frame
 * phase.
 *
 * The frames are concatenated in buf, and
 * their sizes is given in the buflen array.
 *
 * Most of the time, the buf will not be completely
 * filled
 */
struct zmq_dab_message_t
{
    zmq_dab_message_t()
    {
        /* set buf lengths to invalid */
        buflen[0] = -1;
        buflen[1] = -1;
        buflen[2] = -1;
        buflen[3] = -1;

        version = 1;
    }
    uint32_t version;
    int16_t buflen[NUM_FRAMES_PER_ZMQ_MESSAGE];
    /* The head stops here. Use the macro below to calculate
     * the head size.
     */

    uint8_t  buf[NUM_FRAMES_PER_ZMQ_MESSAGE*6144];
};

#define ZMQ_DAB_MESSAGE_HEAD_LENGTH (4 + NUM_FRAMES_PER_ZMQ_MESSAGE*2)

// -------------- ZeroMQ message queue ------------------
class DabOutputZMQ : public DabOutput
{
    public:
        DabOutputZMQ() :
            endpoint_(""),
            zmq_proto_(""), zmq_context_(1),
            zmq_pub_sock_(zmq_context_, ZMQ_PUB),
            zmq_message_ix(0)
        {
            throw std::runtime_error("ERROR: No ZMQ protocol specified !");
        }

        DabOutputZMQ(std::string zmq_proto) :
            endpoint_(""),
            zmq_proto_(zmq_proto), zmq_context_(1),
            zmq_pub_sock_(zmq_context_, ZMQ_PUB),
            zmq_message_ix(0)
        { }


        ~DabOutputZMQ()
        {
            zmq_pub_sock_.close();
        }

        std::string get_info() {
            return "zmq: " + zmq_proto_ + "://" + endpoint_;
        }

        int Open(const char* endpoint);
        int Write(void* buffer, int size);
        int Close();
    private:
        DabOutputZMQ(const DabOutputZMQ& other) :
            zmq_proto_(other.zmq_proto_), zmq_context_(1),
            zmq_pub_sock_(zmq_context_, ZMQ_PUB)
        {
            /* Forbid copy constructor */
        }

        std::string endpoint_;
        std::string zmq_proto_;
        zmq::context_t zmq_context_; // handle for the zmq context
        zmq::socket_t zmq_pub_sock_; // handle for the zmq publisher socket

        zmq_dab_message_t zmq_message;
        int zmq_message_ix;
};

#endif

#endif

