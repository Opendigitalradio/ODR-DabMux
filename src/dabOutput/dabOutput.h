/*
   Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Her Majesty the Queen in
   Right of Canada (Communications Research Center Canada)

   Copyright (C) 2016
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://www.opendigitalradio.org

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

#pragma once

#include "UdpSocket.h"
#include "Log.h"
#include "string.h"
#include <stdexcept>
#include <signal.h>
#include <vector>
#include <chrono>
#include <memory>

#include <unistd.h>
#include <sys/time.h>
#ifndef O_BINARY
# define O_BINARY 0
#endif // O_BINARY
#ifdef HAVE_OUTPUT_ZEROMQ
#  include "zmq.hpp"
#endif
#include "dabOutput/metadata.h"

/** Configuration for EDI output */

// Can represent both unicast and multicast destinations
struct edi_destination_t {
    std::string dest_addr;
    std::string source_addr;
    unsigned int source_port = 0;
    unsigned int ttl = 10;

    std::shared_ptr<UdpSocket> socket;
};

struct edi_configuration_t {
    unsigned chunk_len = 207;        // RSk, data length of each chunk
    unsigned fec       = 0;          // number of fragments that can be recovered
    bool dump          = false;      // dump a file with the EDI packets
    bool verbose       = false;
    bool enable_pft    = false;      // Enable protection and fragmentation
    unsigned int tagpacket_alignment = 0;
    std::vector<edi_destination_t> destinations;
    unsigned int dest_port = 0;      // common destination port, because it's encoded in the transport layer
    unsigned int latency_frames = 0; // if nonzero, enable interleaver with a latency of latency_frames * 24ms

    bool enabled() const { return destinations.size() > 0; }
    bool interleaver_enabled() const { return latency_frames > 0; }
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

        virtual std::string get_info() const = 0;

        virtual void setMetadata(std::shared_ptr<OutputMetadata> &md) = 0;
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

        int Open(const char* filename);
        int Write(void* buffer, int size);
        int Close();

        std::string get_info() const {
            return "file://" + filename_;
        }

        virtual void setMetadata(std::shared_ptr<OutputMetadata> &md) {}
    protected:
        /* Set ETI type according to filename, and return
         * filename without the &type=foo part
         */
        std::string SetEtiType(const std::string& filename);

        std::string filename_;
        int file_;
        EtiFileType type_;
        unsigned long nbFrames_;
};

// ---------- FIFO output ------------
class DabOutputFifo : public DabOutputFile
{
    public:
        DabOutputFifo() : DabOutputFile() {}

        int Open(const char* filename);
        int Write(void* buffer, int size);

        std::string get_info() const {
            return "fifo://" + filename_;
        }

};

// -------------- RAW socket -----------
class DabOutputRaw : public DabOutput
{
    public:
        DabOutputRaw()
        {
            socket_ = -1;
            isCyclades_ = false;
            buffer_ = new unsigned char[6144];
        }

        DabOutputRaw(const DabOutputRaw& other)
        {
            socket_ = other.socket_;
            isCyclades_ = other.isCyclades_;
            buffer_ = new unsigned char[6144];
            memcpy(buffer_, other.buffer_, 6144);
        }

        ~DabOutputRaw() {
            delete[] buffer_;
        }

        const DabOutputRaw operator=(const DabOutputRaw& other) = delete;

        int Open(const char* name);
        int Write(void* buffer, int size);
        int Close();

        std::string get_info() const {
            return "raw://" + filename_;
        }
        virtual void setMetadata(std::shared_ptr<OutputMetadata> &md) {}
    private:
        std::string filename_;
        int socket_;
        bool isCyclades_;
        unsigned char* buffer_;
};

// -------------- UDP ------------------
class DabOutputUdp : public DabOutput
{
    public:
        DabOutputUdp() {
            packet_ = new UdpPacket(6144);
            socket_ = new UdpSocket();
        }

        virtual ~DabOutputUdp() {
            delete socket_;
            delete packet_;
        }

        int Open(const char* name);
        int Write(void* buffer, int size);
        int Close() { return 0; }

        std::string get_info() const {
            return "udp://" + uri_;
        }
        virtual void setMetadata(std::shared_ptr<OutputMetadata> &md) {}
    private:
        // make sure we don't copy this output around
        // the UdpPacket and UdpSocket do not support
        // copying either
        DabOutputUdp(const DabOutputUdp& other) = delete;
        DabOutputUdp operator=(const DabOutputUdp& other) = delete;

        std::string uri_;
        UdpSocket* socket_;
        UdpPacket* packet_;
};

// -------------- TCP ------------------
class TCPDataDispatcher;
class DabOutputTcp : public DabOutput
{
    public:
        int Open(const char* name);
        int Write(void* buffer, int size);
        int Close();

        std::string get_info() const {
            return "tcp://" + uri_;
        }
        virtual void setMetadata(std::shared_ptr<OutputMetadata> &md) {}
    private:
        std::string uri_;

        std::shared_ptr<TCPDataDispatcher> dispatcher_;
};

// -------------- Simul ------------------
class DabOutputSimul : public DabOutput
{
    public:
        int Open(const char* name);
        int Write(void* buffer, int size);
        int Close() { return 0; }

        std::string get_info() const {
            return "simul://" + name_;
        }
        virtual void setMetadata(std::shared_ptr<OutputMetadata> &md) {}
    private:
        std::string name_;
        std::chrono::steady_clock::time_point startTime_;
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

    /* The packet is then followed with metadata appended to it,
     * according to dabOutput/metadata.h
     */
};

#define ZMQ_DAB_MESSAGE_HEAD_LENGTH (4 + NUM_FRAMES_PER_ZMQ_MESSAGE*2)

// -------------- ZeroMQ message queue ------------------
class DabOutputZMQ : public DabOutput
{
    public:
        DabOutputZMQ(const std::string &zmq_proto, bool allow_metadata) :
            endpoint_(""),
            zmq_proto_(zmq_proto), zmq_context_(1),
            zmq_pub_sock_(zmq_context_, ZMQ_PUB),
            zmq_message_ix(0),
            m_allow_metadata(allow_metadata)
        { }

        DabOutputZMQ(const DabOutputZMQ& other) = delete;
        DabOutputZMQ& operator=(const DabOutputZMQ& other) = delete;

        virtual ~DabOutputZMQ()
        {
            zmq_pub_sock_.close();
        }

        std::string get_info(void) const {
            return "zmq: " + zmq_proto_ + "://" + endpoint_;
        }

        int Open(const char* endpoint);
        int Write(void* buffer, int size);
        int Close();
        void setMetadata(std::shared_ptr<OutputMetadata> &md);

    private:
        std::string endpoint_;
        std::string zmq_proto_;
        zmq::context_t zmq_context_; // handle for the zmq context
        zmq::socket_t zmq_pub_sock_; // handle for the zmq publisher socket

        zmq_dab_message_t zmq_message;
        int zmq_message_ix;

        bool m_allow_metadata;
        std::vector<std::shared_ptr<OutputMetadata> > meta_;
};

#endif

