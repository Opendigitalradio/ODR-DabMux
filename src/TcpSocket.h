/*
   Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Her Majesty the Queen in
   Right of Canada (Communications Research Center Canada)

   Copyright (C) 2016
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

#include <boost/optional.hpp>

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

        int close();

        /** Send data over the TCP connection.
         *  @param data The buffer that will be sent.
         *  @param size Number of bytes to send.
         *  return number of bytes sent or -1 if error
         */
        ssize_t send(const void* data, size_t size);

        /** Receive data from the socket.
         *  @param data The buffer that will receive data.
         *  @param size The buffer size.
         *  @return number of bytes received or -1 (SOCKET_ERROR) if error
         */
        ssize_t recv(void* data, size_t size);

        void listen(void);
        TcpSocket accept(void);
        boost::optional<TcpSocket> accept(int timeout_ms);

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

#endif // _TCPSOCKET
