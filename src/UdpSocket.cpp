/*
   Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2015 Matthias P. Braendli
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

#include "UdpSocket.h"

#include <iostream>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

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
int UdpSocket::init()
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
int UdpSocket::clean()
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
UdpSocket::UdpSocket() :
  listenSocket(INVALID_SOCKET)
{
  TRACE_CLASS("UdpSocket", "UdpSocket()");
}


/**
 *  One step constructor.
 *  @param port The port number on which the socket will be bind
 *  @param name The IP address on which the socket will be bind.
 *              It is used to bind the socket on a specific interface if
 *              the computer have many NICs.
 */
UdpSocket::UdpSocket(int port, char *name) :
  listenSocket(INVALID_SOCKET)
{
  TRACE_CLASS("UdpSocket", "UdpSocket(int, char*)");
  create(port, name);
}


/**
 *  This functin set blocking mode. The socket can be blocking or not,
 *  depending of the parametre. By default, the socket is blocking.
 *  @param block If true, set the socket blocking, otherwise set non-blocking
 *  @return 0  if ok
 *          -1 if error
 */
int UdpSocket::setBlocking(bool block)
{
#ifdef _WIN32
	unsigned long res = block ? 0 : 1;
	if (ioctlsocket(listenSocket, FIONBIO, &res) != 0) {
        setInetError("Can't change blocking state of socket");
        return -1;
	}
    return 0;
#else
  int res;
  if (block)
    res = fcntl(listenSocket, F_SETFL, 0);
  else
    res = fcntl(listenSocket, F_SETFL, O_NONBLOCK);
  if (res == SOCKET_ERROR) {
    setInetError("Can't change blocking state of socket");
    return -1;
  }
  return 0;
#endif
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
int UdpSocket::create(int port, char *name)
{
  TRACE_CLASS("UdpSocket", "create(int, char*)");
  if (listenSocket != INVALID_SOCKET)
    closesocket(listenSocket);
  address.setAddress(name);
  address.setPort(port);
  if ((listenSocket = socket(PF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
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


/// Destructor
UdpSocket::~UdpSocket() {
  TRACE_CLASS("UdpSocket", "~UdpSocket()");
  if (listenSocket != INVALID_SOCKET)
    closesocket(listenSocket);
}


/**
 *  Receive an UDP packet.
 *  @param packet The packet that will receive data. The address will be set
 *                to the source address.
 *  @return 0 if ok, -1 if error
 */
int UdpSocket::receive(UdpPacket &packet)
{
  TRACE_CLASS("UdpSocket", "receive(UdpPacket)");
  socklen_t addrSize;
  addrSize = sizeof(*packet.getAddress().getAddress());
  int ret = recvfrom(listenSocket, packet.getData(), packet.getSize() - packet.getOffset(), 0,
		     packet.getAddress().getAddress(), &addrSize);
  if (ret == SOCKET_ERROR) {
    packet.setLength(0);
#ifndef _WIN32
  if (errno == EAGAIN)
    return 0;
#endif
    setInetError("Can't receive UDP packet");
    return -1;
  }
  packet.setLength(ret);
  if (ret == (long)packet.getSize()) {
    packet.setSize(packet.getSize() << 1);
  }
  return 0;
}

/**
 *  Send an UDP packet.
 *  @param packet The UDP packet to be sent. It includes the data and the
 *                destination address
 *  return 0 if ok, -1 if error
 */
int UdpSocket::send(UdpPacket &packet)
{
#ifdef DUMP
  TRACE_CLASS("UdpSocket", "send(UdpPacket)");
#endif
  int ret = sendto(listenSocket, packet.getData(), packet.getLength(), 0,
		   packet.getAddress().getAddress(), sizeof(*packet.getAddress().getAddress()));
  if (ret == SOCKET_ERROR
#ifndef _WIN32
      && errno != ECONNREFUSED
#endif
      ) {
    setInetError("Can't send UDP packet");
    return -1;
  }
  return 0;
}


/**
 *  Send an UDP packet
 *
 *  return 0 if ok, -1 if error
 */
int UdpSocket::send(std::vector<uint8_t> data, InetAddress destination)
{
#ifdef DUMP
  TRACE_CLASS("UdpSocket", "send(vector<uint8_t>)");
#endif
  int ret = sendto(listenSocket, &data[0], data.size(), 0,
		   destination.getAddress(), sizeof(*destination.getAddress()));
  if (ret == SOCKET_ERROR
#ifndef _WIN32
      && errno != ECONNREFUSED
#endif
      ) {
    setInetError("Can't send UDP packet");
    return -1;
  }
  return 0;
}


/**
 *  Must be called to receive data on a multicast address.
 *  @param groupname The multica
st address to join.
 *  @return 0 if ok, -1 if error
 */
int UdpSocket::joinGroup(char* groupname)
{
  TRACE_CLASS("UdpSocket", "joinGroup(char*)");
#ifdef _WIN32
  ip_mreq group;
#else
  ip_mreqn group;
#endif
  if ((group.imr_multiaddr.s_addr = inet_addr(groupname)) == INADDR_NONE) {
    setInetError(groupname);
    return -1;
  }
  if (!IN_MULTICAST(ntohl(group.imr_multiaddr.s_addr))) {
    setInetError("Not a multicast address");
    return -1;
  }
#ifdef _WIN32
  group.imr_interface.s_addr = 0;
  if (setsockopt(listenSocket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&group, sizeof(group))
      == SOCKET_ERROR) {
    setInetError("Can't join multicast group");
    return -1;
  }
#else
  group.imr_address.s_addr = htons(INADDR_ANY);;
  group.imr_ifindex = 0;
  if (setsockopt(listenSocket, SOL_IP, IP_ADD_MEMBERSHIP, &group, sizeof(group))
      == SOCKET_ERROR) {
    setInetError("Can't join multicast group");
  }
#endif
  return 0;
}

int UdpSocket::setMulticastTTL(int ttl)
{
    if (setsockopt(listenSocket, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl))
            == SOCKET_ERROR) {
        setInetError("Can't set ttl");
        return -1;
    }

    return 0;
}

int UdpSocket::setMulticastSource(const char* source_addr)
{
    struct in_addr addr;
    if (inet_aton(source_addr, &addr) == 0) {
        setInetError("Can't parse source address");
        return -1;
    }

    if (setsockopt(listenSocket, IPPROTO_IP, IP_MULTICAST_IF, &addr, sizeof(addr))
            == SOCKET_ERROR) {
        setInetError("Can't set source address");
        return -1;
    }

    return 0;
}


/**
 *  Constructs an UDP packet.
 *  @param initSize The initial size of the data buffer
 */
UdpPacket::UdpPacket(unsigned int initSize) :
  dataBuf(new char[initSize]),
  length(0),
  size(initSize),
  offset(0)
{
  TRACE_CLASS("UdpPacket", "UdpPacket(unsigned int)");
  if (dataBuf == NULL)
    size = 0;
}


/// Destructor
UdpPacket::~UdpPacket()
{
  TRACE_CLASS("UdpPacket", "~UdpPacket()");
  if (dataBuf != NULL) {
    delete []dataBuf;
    dataBuf = NULL;
  }
}
  

/**
 *  Changes size of the data buffer size. \a Length + \a offset data will be copied
 *  in the new buffer.
 *  @warning The pointer to data will be changed
 *  @param newSize The new data buffer size
 */
void UdpPacket::setSize(unsigned newSize)
{
  TRACE_CLASS("UdpPacket", "setSize(unsigned)");
  char *tmp = new char[newSize];
  if (length > newSize)
    length = newSize;
  if (tmp) {
    memcpy(tmp, dataBuf, length);
    delete []dataBuf;
    dataBuf = tmp;
    size = newSize;
  }
}


/**
 *  Give the pointer to data. It is ajusted with the \a offset.
 *  @warning This pointer change. when the \a size of the buffer and the \a offset change.
 *  @return The pointer
 */
char *UdpPacket::getData()
{
  return dataBuf + offset;
}


/**
 *  Add some data at the end of data buffer and adjust size.
 *  @param data Pointer to the data to add
 *  @param size Size in bytes of new data
 */
void UdpPacket::addData(const void *data, unsigned size)
{
  if (length + size > this->size) {
    setSize(this->size << 1);
  }
  memcpy(dataBuf + length, data, size);
  length += size;
}


/**
 *  Returns the length of useful data. Data before the \a offset are ignored.
 *  @return The data length
 */
unsigned long UdpPacket::getLength()
{
  return length - offset;
}


/**
 *  Returns the size of the data buffer.
 *  @return The data buffer size
 */
unsigned long UdpPacket::getSize()
{
  return size;
}


/**
 *  Returns the offset value.
 *  @return The offset value
 */
unsigned long UdpPacket::getOffset()
{
  return offset;
}


/**
 *  Sets the data length value. Data before the \a offset are ignored.
 *  @param len The new length of data
 */
void UdpPacket::setLength(unsigned long len)
{
  length = len + offset;
}


/**
 *  Sets the data offset. Data length is ajusted to ignore data before the \a offset.
 *  @param val The new data offset.
 */
void UdpPacket::setOffset(unsigned long val)
{
  offset = val;
  if (offset > length)
    length = offset;
}


/**
 *  Returns the UDP address of the data.
 *  @return The UDP address
 */
InetAddress &UdpPacket::getAddress()
{
  return address;
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
