/*
   Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Her Majesty the Queen in
   Right of Canada (Communications Research Center Canada)

   Copyright (C) 2019
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://www.opendigitalradio.org
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

#include "TcpSocket.h"
#include "Log.h"
#include <iostream>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <signal.h>
#include <errno.h>
#include <poll.h>
#include <thread>

using namespace std;

using vec_u8 = std::vector<uint8_t>;


TcpSocket::TcpSocket() :
    m_sock(INVALID_SOCKET)
{
}

TcpSocket::TcpSocket(int port, const string& name) :
    m_sock(INVALID_SOCKET)
{
    if (port) {
        if ((m_sock = socket(PF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
            throw std::runtime_error("Can't create socket");
        }

        reuseopt_t reuse = 1;
        if (setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse))
                == SOCKET_ERROR) {
            throw std::runtime_error("Can't reuse address");
        }

#if defined(HAVE_SO_NOSIGPIPE)
        int val = 1;
        if (setsockopt(m_sock, SOL_SOCKET, SO_NOSIGPIPE, &val, sizeof(val))
                == SOCKET_ERROR) {
            throw std::runtime_error("Can't set SO_NOSIGPIPE");
        }
#endif

        m_own_address.setAddress(name);
        m_own_address.setPort(port);

        if (::bind(m_sock, m_own_address.getAddress(), sizeof(sockaddr_in)) == SOCKET_ERROR) {
            ::close(m_sock);
            m_sock = INVALID_SOCKET;
            throw std::runtime_error("Can't bind socket");
        }
    }
}

TcpSocket::TcpSocket(SOCKET sock, InetAddress own, InetAddress remote) :
    m_own_address(own),
    m_remote_address(remote),
    m_sock(sock) { }

// The move constructors must ensure the moved-from
// TcpSocket won't destroy our socket handle
TcpSocket::TcpSocket(TcpSocket&& other)
{
    m_sock = other.m_sock;
    other.m_sock = INVALID_SOCKET;

    m_own_address = other.m_own_address;
    m_remote_address = other.m_remote_address;
}

TcpSocket& TcpSocket::operator=(TcpSocket&& other)
{
    m_sock = other.m_sock;
    other.m_sock = INVALID_SOCKET;

    m_own_address = other.m_own_address;
    m_remote_address = other.m_remote_address;
    return *this;
}

/**
 *  Close the underlying socket.
 *  @return 0  if ok
 *          -1 if error
 */
int TcpSocket::close()
{
    if (m_sock != INVALID_SOCKET) {
        int res = ::close(m_sock);
        if (res != 0) {
            setInetError("Can't close socket");
            return -1;
        }
        m_sock = INVALID_SOCKET;
    }
    return 0;
}

TcpSocket::~TcpSocket()
{
    close();
}

bool TcpSocket::isValid()
{
    return m_sock != INVALID_SOCKET;
}

ssize_t TcpSocket::recv(void* data, size_t size)
{
    ssize_t ret = ::recv(m_sock, (char*)data, size, 0);
    if (ret == SOCKET_ERROR) {
        stringstream ss;
        ss << "TCP Socket recv error: " << strerror(errno);
        throw std::runtime_error(ss.str());
    }
    return ret;
}


ssize_t TcpSocket::send(const void* data, size_t size, int timeout_ms)
{
    if (timeout_ms) {
        struct pollfd fds[1];
        fds[0].fd = m_sock;
        fds[0].events = POLLOUT;

        const int retval = poll(fds, 1, timeout_ms);

        if (retval == -1) {
            stringstream ss;
            ss << "TCP Socket send error on poll(): " << strerror(errno);
            throw std::runtime_error(ss.str());
        }
        else if (retval == 0) {
            // Timed out
            return 0;
        }
    }

    /* On Linux, the MSG_NOSIGNAL flag ensures that the process would not
     * receive a SIGPIPE and die.
     * Other systems have SO_NOSIGPIPE set on the socket for the same effect. */
#if defined(HAVE_MSG_NOSIGNAL)
    const int flags = MSG_NOSIGNAL;
#else
    const int flags = 0;
#endif
    const ssize_t ret = ::send(m_sock, (const char*)data, size, flags);

    if (ret == SOCKET_ERROR) {
        stringstream ss;
        ss << "TCP Socket send error: " << strerror(errno);
        throw std::runtime_error(ss.str());
    }
    return ret;
}

void TcpSocket::listen()
{
    if (::listen(m_sock, 1) == SOCKET_ERROR) {
        stringstream ss;
        ss << "TCP Socket listen error: " << strerror(errno);
        throw std::runtime_error(ss.str());
    }
}

TcpSocket TcpSocket::accept()
{
    InetAddress remote_addr;
    socklen_t addrLen = sizeof(sockaddr_in);

    SOCKET socket = ::accept(m_sock, remote_addr.getAddress(), &addrLen);
    if (socket == SOCKET_ERROR) {
        stringstream ss;
        ss << "TCP Socket accept error: " << strerror(errno);
        throw std::runtime_error(ss.str());
    }
    else {
        TcpSocket client(socket, m_own_address, remote_addr);
        return client;
    }
}

TcpSocket TcpSocket::accept(int timeout_ms)
{
    struct pollfd fds[1];
    fds[0].fd = m_sock;
    fds[0].events = POLLIN | POLLOUT;

    int retval = poll(fds, 1, timeout_ms);

    if (retval == -1) {
        stringstream ss;
        ss << "TCP Socket accept error: " << strerror(errno);
        throw std::runtime_error(ss.str());
    }
    else if (retval) {
        return accept();
    }
    else {
        TcpSocket invalidsock(0, "");
        return invalidsock;
    }
}


InetAddress TcpSocket::getOwnAddress() const
{
    return m_own_address;
}

InetAddress TcpSocket::getRemoteAddress() const
{
    return m_remote_address;
}


TCPConnection::TCPConnection(TcpSocket&& sock) :
            queue(),
            m_running(true),
            m_sender_thread(),
            m_sock(move(sock))
{
    auto addr = m_sock.getRemoteAddress();
    etiLog.level(debug) << "New TCP Connection from " <<
        addr.getHostAddress() << ":" << addr.getPort();
    m_sender_thread = std::thread(&TCPConnection::process, this);
}

TCPConnection::~TCPConnection()
{
    m_running = false;
    vec_u8 termination_marker;
    queue.push(termination_marker);
    m_sender_thread.join();
}

void TCPConnection::process()
{
    while (m_running) {
        vec_u8 data;
        queue.wait_and_pop(data);

        if (data.empty()) {
            // empty vector is the termination marker
            m_running = false;
            break;
        }

        try {
            ssize_t remaining = data.size();
            const uint8_t *buf = reinterpret_cast<const uint8_t*>(data.data());
            const int timeout_ms = 10; // Less than one ETI frame

            while (m_running and remaining > 0) {
                const ssize_t sent = m_sock.send(buf, remaining, timeout_ms);
                if (sent < 0 or sent > remaining) {
                    throw std::logic_error("Invalid TcpSocket::send() return value");
                }
                remaining -= sent;
                buf += sent;
            }
        }
        catch (const std::runtime_error& e) {
            m_running = false;
        }
    }

    auto addr = m_sock.getRemoteAddress();
    etiLog.level(debug) << "Dropping TCP Connection from " <<
        addr.getHostAddress() << ":" << addr.getPort();
}


TCPDataDispatcher::TCPDataDispatcher(size_t max_queue_size) :
    m_max_queue_size(max_queue_size)
{
}

TCPDataDispatcher::~TCPDataDispatcher()
{
    m_running = false;
    m_connections.clear();
    m_listener_socket.close();
    m_listener_thread.join();
}

void TCPDataDispatcher::start(int port, const string& address)
{
    TcpSocket sock(port, address);
    m_listener_socket = move(sock);

    m_running = true;
    m_listener_thread = std::thread(&TCPDataDispatcher::process, this);
}

void TCPDataDispatcher::write(const vec_u8& data)
{
    for (auto& connection : m_connections) {
        connection.queue.push(data);
    }

    m_connections.remove_if(
            [&](const TCPConnection& conn){ return conn.queue.size() > m_max_queue_size; });
}

void TCPDataDispatcher::process()
{
    try {
        m_listener_socket.listen();

        const int timeout_ms = 1000;

        while (m_running) {
            // Add a new TCPConnection to the list, constructing it from the client socket
            auto sock = m_listener_socket.accept(timeout_ms);
            if (sock.isValid()) {
                m_connections.emplace(m_connections.begin(), move(sock));
            }
        }
    }
    catch (const std::runtime_error& e) {
        etiLog.level(error) << "TCPDataDispatcher caught runtime error: " << e.what();
        m_running = false;
    }
}

