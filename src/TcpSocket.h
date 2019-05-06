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

#ifndef _TCPSOCKET
#define _TCPSOCKET

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include "InetAddress.h"
#include "ThreadsafeQueue.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#define SOCKET           int
#define INVALID_SOCKET   -1
#define SOCKET_ERROR     -1
#define reuseopt_t       int

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <thread>
#include <list>

/**
 *  This class represents a TCP socket.
 */
class TcpSocket
{
    public:
        /** Create a new socket that does nothing */
        TcpSocket();

        /** Create a new socket listening for incoming connections.
         *  @param port The port number on which the socket will listen.
         *  @param name The IP address on which the socket will be bound.
         *              It is used to bind the socket on a specific interface if
         *              the computer have many NICs.
         */
        TcpSocket(int port, const std::string& name);
        ~TcpSocket();
        TcpSocket(TcpSocket&& other);
        TcpSocket& operator=(TcpSocket&& other);
        TcpSocket(const TcpSocket& other) = delete;
        TcpSocket& operator=(const TcpSocket& other) = delete;

        bool isValid(void);

        int close(void);

        /** Send data over the TCP connection.
         *  @param data The buffer that will be sent.
         *  @param size Number of bytes to send.
         *  @param timeout_ms number of milliseconds before timeout, or 0 for infinite timeout
         *  return number of bytes sent, 0 on timeout, or throws runtime_error.
         */
        ssize_t send(const void* data, size_t size, int timeout_ms=0);

        /** Receive data from the socket.
         *  @param data The buffer that will receive data.
         *  @param size The buffer size.
         *  @return number of bytes received or -1 (SOCKET_ERROR) if error
         */
        ssize_t recv(void* data, size_t size);

        void listen(void);
        TcpSocket accept(void);

        /* Returns either valid socket if a connection was
         * accepted before the timeout expired, or an invalid
         * socket otherwise.
         */
        TcpSocket accept(int timeout_ms);

        /** Retrieve address this socket is bound to */
        InetAddress getOwnAddress() const;
        InetAddress getRemoteAddress() const;

    private:
        TcpSocket(SOCKET sock, InetAddress own, InetAddress remote);

        /// The address on which the socket is bound.
        InetAddress m_own_address;
        InetAddress m_remote_address;
        /// The low-level socket used by system functions.
        SOCKET m_sock;
};

/* Helper class for TCPDataDispatcher, contains a queue of pending data and
 * a sender thread. */
class TCPConnection
{
    public:
        TCPConnection(TcpSocket&& sock);
        TCPConnection(const TCPConnection&) = delete;
        TCPConnection& operator=(const TCPConnection&) = delete;
        ~TCPConnection();

        ThreadsafeQueue<std::vector<uint8_t> > queue;

    private:
        std::atomic<bool> m_running;
        std::thread m_sender_thread;
        TcpSocket m_sock;

        void process(void);
};

/* Send a TCP stream to several destinations, and automatically disconnect destinations
 * whose buffer overflows.
 */
class TCPDataDispatcher
{
    public:
        TCPDataDispatcher(size_t max_queue_size);
        ~TCPDataDispatcher();
        TCPDataDispatcher(const TCPDataDispatcher&) = delete;
        TCPDataDispatcher& operator=(const TCPDataDispatcher&) = delete;

        void start(int port, const std::string& address);
        void write(const std::vector<uint8_t>& data);

    private:
        void process(void);

        size_t m_max_queue_size;

        std::atomic<bool> m_running;
        std::thread m_listener_thread;
        TcpSocket m_listener_socket;
        std::list<TCPConnection> m_connections;
};

#endif // _TCPSOCKET
