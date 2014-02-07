/*
   Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Her Majesty the Queen in
   Right of Canada (Communications Research Center Canada)
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

#include "TcpServer.h"
#include <iostream>
#include <stdio.h>
#include <errno.h>

#ifdef _WIN32
#else
#   include <sys/socket.h>
#endif

#ifdef TRACE_ON
# ifndef TRACE_CLASS
#  define TRACE_CLASS(class, func) cout <<"-" <<(class) <<"\t(" <<this <<")::" <<(func) <<endl
#  define TRACE_STATIC(class, func) cout <<"-" <<(class) <<"\t(static)::" <<(func) <<endl
# endif
#else
# ifndef TRACE_CLASS
#  define TRACE_CLASS(class, func)
#  define TRACE_STATIC(class, func)
# endif
#endif


/// Must be call once before doing any operation on sockets
int TcpServer::init()
{
#ifdef _WIN32
  WSADATA wsaData;
  WORD wVersionRequested = wVersionRequested = MAKEWORD( 2, 2 );
  
  int res = WSAStartup( wVersionRequested, &wsaData );
  if (res) {
    setInetError("Can't initialize winsock");
    return -1;
  }
#endif
  return 0;
}


/// Must be call once before leaving application
int TcpServer::clean()
{
#ifdef _WIN32
  int res = WSACleanup();
  if (res) {
    setInetError("Can't initialize winsock");
    return -1;
  }
#endif
  return 0;
}


/**
 *  Two step constructor. Create must be called prior to use this
 *  socket.
 */
TcpServer::TcpServer() :
  listenSocket(INVALID_SOCKET)
{
  TRACE_CLASS("TcpServer", "TcpServer()");
}


/**
 *  One step constructor.
 *  @param port The port number on which the socket will be bind
 *  @param name The IP address on which the socket will be bind.
 *              It is used to bind the socket on a specific interface if
 *              the computer have many NICs.
 */
TcpServer::TcpServer(int port, const char *name) :
  listenSocket(INVALID_SOCKET)
{
  TRACE_CLASS("TcpServer", "TcpServer(int, char*)");
  create(port, name);
}


/**
 *  Two step initializer. This function must be called after the constructor
 *  without argument as been called.
 *  @param port The port number on which the socket will be bind
 *  @param name The IP address on which the socket will be bind.
 *              It is used to bind the socket on a specific interface if
 *              the computer have many NICs.
 *  @return 0  if ok
 *          -1 if error
 */
int TcpServer::create(int port, const char *name)
{
  TRACE_CLASS("TcpServer", "create(int, char*)");
  if (listenSocket != INVALID_SOCKET)
    closesocket(listenSocket);
  address.setAddress(name);
  address.setPort(port);
  if ((listenSocket = socket(PF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
    setInetError("Can't create socket");
    return -1;
  }
  reuseopt_t reuse = 1;
  if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse))
      == SOCKET_ERROR) {
    setInetError("Can't reuse address");
    return -1;
  }
  if (bind(listenSocket, address.getAddress(), sizeof(sockaddr_in)) == SOCKET_ERROR) {
    setInetError("Can't bind socket");
    closesocket(listenSocket);
    listenSocket = INVALID_SOCKET;
    return -1;
  }
  return 0;
}


/**
 *  Close the underlying socket.
 *  @return 0  if ok
 *          -1 if error
 */
int TcpServer::close()
{
    TRACE_CLASS("TcpServer", "close()");
    if (listenSocket != INVALID_SOCKET) {
        int res = closesocket(listenSocket);
        if (res != 0) {
            setInetError("Can't close socket");
            return -1;
        }
        listenSocket = INVALID_SOCKET;
    }
    return 0;    
}


/// Destructor
TcpServer::~TcpServer()
{
  TRACE_CLASS("TcpServer", "~TcpServer()");
  close();
}


int TcpServer::listen()
{
  TRACE_CLASS("TcpServer", "listen()");
  if (::listen(listenSocket, 1) == SOCKET_ERROR) {
    setInetError("Can't listen on socket");
    return -1;
  }
  return 0;
}


TcpSocket* TcpServer::accept()
{
  SOCKET socket;
  TcpSocket* client = NULL;
  InetAddress addr;
  socklen_t addrLen = sizeof(sockaddr_in);

  socket = ::accept(listenSocket, addr.getAddress(), &addrLen);
  if (socket == SOCKET_ERROR) {
    setInetError("Can't accept connection on socket");
  } else {
    client = new TcpSocket();
    client->setSocket(socket, addr);
  }
  return client;
}

/*
WSAEINTR
WSAEBADF
WSAEACCES
WSAEFAULT
WSAEINVAL
WSAEMFILE
WSAEWOULDBLOCK
WSAEINPROGRESS
WSAEALREADY
WSAENOTSOCK
WSAEDESTADDRREQ
WSAEMSGSIZE
WSAEPROTOTYPE
WSAENOPROTOOPT
WSAEPROTONOSUPPORT
WSAESOCKTNOSUPPORT
WSAEOPNOTSUPP
WSAEPFNOSUPPORT
WSAEAFNOSUPPORT
WSAEADDRINUSE
WSAEADDRNOTAVAIL
WSAENETDOWN
WSAENETUNREACH
WSAENETRESET
WSAECONNABORTED
WSAECONNRESET
WSAENOBUFS
WSAEISCONN
WSAENOTCONN
WSAESHUTDOWN
WSAETOOMANYREFS
WSAETIMEDOUT
WSAECONNREFUSED
WSAELOOP
WSAENAMETOOLONG
WSAEHOSTDOWN
WSAEHOSTUNREACH
WSAENOTEMPTY
WSAEPROCLIM
WSAEUSERS
WSAEDQUOT
WSAESTALE
WSAEREMOTE
WSAEDISCON
WSASYSNOTREADY
WSAVERNOTSUPPORTED
WSANOTINITIALISED
*/
