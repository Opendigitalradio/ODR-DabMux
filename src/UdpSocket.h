/*
   Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)
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

#ifndef _UDPSOCKET
#define _UDPSOCKET

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include "InetAddress.h"
#ifdef _WIN32
# include <winsock.h>
# define socklen_t        int
# define reuseopt_t       char
#else
# include <sys/socket.h>
# include <netinet/in.h>
# include <unistd.h>
# include <netdb.h>
# include <arpa/inet.h>
# include <pthread.h>
# define SOCKET           int
# define INVALID_SOCKET   -1
# define SOCKET_ERROR     -1
# define reuseopt_t       int
#endif
//#define INVALID_PORT      -1

#include <stdlib.h>
#include <iostream>
class UdpPacket;


/**
 *  This class represents a socket for sending and receiving UDP packets.
 *
 *  A UDP socket is the sending or receiving point for a packet delivery service.
 *  Each packet sent or received on a datagram socket is individually
 *  addressed and routed. Multiple packets sent from one machine to another may
 *  be routed differently, and may arrive in any order. 
 *  @author Pascal Charest pascal.charest@crc.ca
 */
class UdpSocket {
 public:
  UdpSocket();
  UdpSocket(int port, char *name = NULL);
  ~UdpSocket();

  static int init();
  static int clean();

  int create(int port = 0, char *name = NULL);

  int send(UdpPacket &packet);
  int receive(UdpPacket &packet);
  int joinGroup(char* groupname);
  /**
   *  Connects the socket on a specific address. Only data from this address
   *  will be received.
   *  @param addr The address to connect the socket
   *  @warning Not implemented yet.
   */
  void connect(InetAddress &addr);
  int setBlocking(bool block);

 protected:
  /// The address on which the socket is binded.
  InetAddress address;
  /// The low-level socket used by system functions.
  SOCKET listenSocket;
};

/**
 *  This class represents a UDP packet. 
 *
 *  UDP packets are used to implement a connectionless packet delivery service.
 *  Each message is routed from one machine to another based solely on
 *  information contained within that packet. Multiple packets sent from one
 *  machine to another might be routed differently, and might arrive in any order. 
 *  @author Pascal Charest pascal.charest@crc.ca
 */
class UdpPacket {
 public:
  UdpPacket(unsigned int initSize = 1024);
  // Not implemented
  UdpPacket(const UdpPacket& packet);
  ~UdpPacket();

  char *getData();
  void addData(void *data, unsigned size);
  unsigned long getLength();
  unsigned long getSize();
  unsigned long getOffset();
  void setLength(unsigned long len);
  void setOffset(unsigned long val);
  void setSize(unsigned newSize);
  InetAddress &getAddress();
  // Not implemented
  const UdpPacket &operator=(const UdpPacket&);
  
 private:
  char *dataBuf;
  unsigned long length, size, offset;
  InetAddress address;
};

#endif // _UDPSOCKET
