/*
   Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Her Majesty the Queen in
   Right of Canada (Communications Research Center Canada)

   Copyright (C) 2016
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
#include <boost/thread.hpp>
#include "ThreadsafeQueue.h"
#include "TcpSocket.h"

using namespace std;

using vec_u8 = std::vector<uint8_t>;

// In ETI one element would be an ETI frame of 6144 bytes.
// 250 frames correspond to 6 seconds. This is mostly here
// to ensure we do not accumulate data for faulty sockets, delay
// management has to be done on the receiver end.
const size_t MAX_QUEUED_ELEMS = 250;

class TCPConnection
{
    public:
        TCPConnection(TcpSocket&& sock) :
            queue(),
            m_running(true),
            m_sender_thread(),
            m_sock(move(sock)) {
                auto addr = m_sock.getRemoteAddress();
                etiLog.level(debug) << "New TCP Connection from " <<
                    addr.getHostAddress() << ":" << addr.getPort();
                m_sender_thread = boost::thread(&TCPConnection::process, this, 0);
            }

        ~TCPConnection() {
            m_running = false;
            queue.notify();
            m_sender_thread.join();
        }

        ThreadsafeQueue<vec_u8> queue;

        bool is_overloaded(void) const {
            return queue.size() > MAX_QUEUED_ELEMS;
        }

    private:
        TCPConnection(const TCPConnection& other) = delete;
        TCPConnection& operator=(const TCPConnection& other) = delete;

        atomic<bool> m_running;
        boost::thread m_sender_thread;
        TcpSocket m_sock;

        void process(long) {
            while (m_running) {
                vec_u8 data;
                queue.wait_and_pop(data);

                try {
                    m_sock.send(&data[0], data.size());
                }
                catch (std::runtime_error& e) {
                    m_running = false;
                }
            }
        }
};

class TCPDataDispatcher
{
    public:
        ~TCPDataDispatcher() {
            m_running = false;
            m_connections.clear();
            m_listener_socket.close();
            m_listener_thread.join();
        }

        void start(int port, const string& address) {
            TcpSocket sock(port, address);
            m_listener_socket = move(sock);

            m_running = true;
            m_listener_thread = boost::thread(&TCPDataDispatcher::process, this, 0);
        }

        void Write(const vec_u8& data) {
            for (auto& connection : m_connections) {
                connection.queue.push(data);
            }

            m_connections.remove_if([](TCPConnection& conn){ return conn.is_overloaded(); });
        }

    private:
        void process(long) {
            m_listener_socket.listen();

            while (m_running) {
                // Add a new TCPConnection to the list, constructing it from the client socket
                m_connections.emplace(m_connections.begin(), m_listener_socket.accept());
            }
        }

        atomic<bool> m_running;
        boost::thread m_listener_thread;
        TcpSocket m_listener_socket;
        std::list<TCPConnection> m_connections;
};

DabOutputTcp::~DabOutputTcp()
{
    if (dispatcher_) {
        delete dispatcher_;
        dispatcher_ = nullptr;
    }
}

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

    if (success) {
        dispatcher_ = new TCPDataDispatcher();
        dispatcher_->start(port, address);
    }
    else {
        stringstream ss;
        ss << "Could not parse TCP output address " << name;
        throw std::runtime_error(ss.str());
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

    dispatcher_->Write(data);
    return size;
}


int DabOutputTcp::Close()
{
    return 0;
}
