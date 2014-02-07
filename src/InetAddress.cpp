/*
   Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)
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

#include "InetAddress.h"
#include <iostream>
#include <stdio.h>

#ifdef _WIN32
#else
# include <errno.h>
# include <string.h>
#endif

#ifdef TRACE_ON
# ifndef TRACE_CLASS
#  define TRACE_CLASS(clas, func) cout <<"-" <<(clas) <<"\t(" <<this <<")::" <<(func) <<endl
#  define TRACE_STATIC(clas, func) cout <<"-" <<(clas) <<"\t(static)::" <<(func) <<endl
# endif
#else
# ifndef TRACE_CLASS
#  define TRACE_CLASS(clas, func)
#  define TRACE_STATIC(clas, func)
# endif
#endif


int inetErrNo = 0;
const char *inetErrMsg = NULL;
const char *inetErrDesc = NULL;


/**
 *  Constructs an IP address.
 *  @param port The port of this address
 *  @param name The name of this address
 */
InetAddress::InetAddress(int port, const char* name) {
  TRACE_CLASS("InetAddress", "InetAddress(int, char)");
  addr.sin_family = PF_INET;
  addr.sin_addr.s_addr = htons(INADDR_ANY);
  addr.sin_port = htons(port);
  if (name)
    setAddress(name);
}


/**
 *  Constructs a copy of inet
 *  @param inet The address to be copied
 */
InetAddress::InetAddress(const InetAddress &inet) {
  TRACE_CLASS("InetAddress", "InetAddress(InetAddress)");
  memcpy(&addr, &inet.addr, sizeof(addr));
}


/// Destructor
InetAddress::~InetAddress() {
  TRACE_CLASS("InetAddress" ,"~InetAddress()");
}


/// Returns the raw IP address of this InetAddress object.
sockaddr *InetAddress::getAddress() {
  TRACE_CLASS("InetAddress", "getAddress()");
  return (sockaddr *)&addr;
}


/// Return the port of this address.
int InetAddress::getPort()
{
  TRACE_CLASS("InetAddress", "getPort()");
  return ntohs(addr.sin_port);
}


/**
 *  Returns the IP address string "%d.%d.%d.%d".
 *  @return IP address
 */
const char *InetAddress::getHostAddress() {
  TRACE_CLASS("InetAddress", "getHostAddress()");
  return inet_ntoa(addr.sin_addr);
}


/// Returns true if this address is multicast
bool InetAddress::isMulticastAddress() {
  TRACE_CLASS("InetAddress", "isMulticastAddress()");
  return IN_MULTICAST(ntohl(addr.sin_addr.s_addr));		// a modifier
}


/**
 *  Set the port number
 *  @param port The new port number
 */
void InetAddress::setPort(int port)
{
  TRACE_CLASS("InetAddress", "setPort(int)");
  addr.sin_port = htons(port);
}


/**
 *  Set the address
 *  @param name The new address name
 *  @return 0  if ok
 *          -1 if error
 */
int InetAddress::setAddress(const char *name)
{
  TRACE_CLASS("InetAddress", "setAddress(char*)");
  if (name) {
    if (atoi(name)) {   // If it start with a number
      if ((addr.sin_addr.s_addr = inet_addr(name)) == INADDR_NONE) {
	addr.sin_addr.s_addr = htons(INADDR_ANY);
	inetErrNo = 0;
	inetErrMsg = "Invalid address";
	inetErrDesc = name;
	return -1;
      }
    } else {            // Assume it's a real name
      hostent *host = gethostbyname(name);
      if (host) {
	addr.sin_addr = *(in_addr *)(host->h_addr);
      } else {
	addr.sin_addr.s_addr = htons(INADDR_ANY);
	inetErrNo = 0;
	inetErrMsg = "Could not find address";
	inetErrDesc = name;
	return -1;
      }
    }
  } else {
    addr.sin_addr.s_addr = INADDR_ANY;
  }
  return 0;
}


void setInetError(const char* description)
{
  inetErrNo = 0;
#ifdef _WIN32
  inetErrNo = WSAGetLastError();
  switch (inetErrNo) {
  case WSANOTINITIALISED:
    inetErrMsg = "WSANOTINITIALISED A successful WSAStartup must occur before using this function.";
    break;
  case WSAENETDOWN:
    inetErrMsg = "WSAENETDOWN The network subsystem has failed.";
    break;
  case WSAEFAULT:
    inetErrMsg = "WSAEFAULT The buf or from parameters are not part of the user address space, or the fromlen parameter is too small to accommodate the peer address.";
    break;
  case WSAEINTR:
    inetErrMsg = "WSAEINTR The (blocking) call was canceled through WSACancelBlockingCall.";
    break;
  case WSAEINPROGRESS:
    inetErrMsg = "WSAEINPROGRESS A blocking Windows Sockets 1.1 call is in progress, or the service provider is still processing a callback function.";
    break;
  case WSAEINVAL:
    inetErrMsg = "WSAEINVAL The socket has not been bound with bind, or an unknown flag was specified, or MSG_OOB was specified for a socket with SO_OOBINLINE enabled, or (for byte stream-style sockets only) len was zero or negative.";
    break;
  case WSAEISCONN:
    inetErrMsg = "WSAEISCONN The socket is connected. This function is not permitted with a connected socket, whether the socket is connection-oriented or connectionless.";
    break;
  case WSAENETRESET:
    inetErrMsg = "WSAENETRESET The connection has been broken due to the \"keep-alive\" activity detecting a failure while the operation was in progress.";
    break;
  case WSAENOTSOCK:
    inetErrMsg = "WSAENOTSOCK The descriptor is not a socket.";
    break;
  case WSAEOPNOTSUPP:
    inetErrMsg = "WSAEOPNOTSUPP MSG_OOB was specified, but the socket is not stream-style such as type SOCK_STREAM, out-of-band data is not supported in the communication domain associated with this socket, or the socket is unidirectional and supports only send operations.";
    break;
  case WSAESHUTDOWN:
    inetErrMsg = "WSAESHUTDOWN The socket has been shut down; it is not possible to recvfrom on a socket after shutdown has been invoked with how set to SD_RECEIVE or SD_BOTH.";
    break;
  case WSAEWOULDBLOCK:
    inetErrMsg = "WSAEWOULDBLOCK The socket is marked as nonblocking and the recvfrom operation would block.";
    break;
  case WSAEMSGSIZE:
    inetErrMsg = "WSAEMSGSIZE The message was too large to fit into the specified buffer and was truncated.";
    break;
  case WSAETIMEDOUT:
    inetErrMsg = "WSAETIMEDOUT The connection has been dropped, because of a network failure or because the system on the other end went down without notice.";
    break;
  case WSAECONNRESET:
    inetErrMsg = "WSAECONNRESET";
    break;
  case WSAEACCES:
    inetErrMsg = "WSAEACCES The requested address is a broadcast address, but the appropriate flag was not set. Call setsockopt with the SO_BROADCAST parameter to allow the use of the broadcast address.";
    break;
  case WSAENOBUFS:
    inetErrMsg = "WSAENOBUFS No buffer space is available.";
    break;
  case WSAENOTCONN:
    inetErrMsg = "WSAENOTCONN The socket is not connected (connection-oriented sockets only)";
    break;
  case WSAEHOSTUNREACH:
    inetErrMsg = "WSAEHOSTUNREACH The remote host cannot be reached from this host at this time.";
    break;
  case WSAECONNABORTED:
    inetErrMsg = "WSAECONNABORTED The virtual circuit was terminated due to a time-out or other failure. The application should close the socket as it is no longer usable.";
    break;
  case WSAEADDRNOTAVAIL:
    inetErrMsg = "WSAEADDRNOTAVAIL The remote address is not a valid address, for example, ADDR_ANY.";
    break;
  case WSAEAFNOSUPPORT:
    inetErrMsg = "WSAEAFNOSUPPORT Addresses in the specified family cannot be used with this socket.";
    break;
  case WSAEDESTADDRREQ:
    inetErrMsg = "WSAEDESTADDRREQ A destination address is required.";
    break;
  case WSAENETUNREACH:
    inetErrMsg = "WSAENETUNREACH The network cannot be reached from this host at this time.";
    break;
  case WSAEMFILE:
	inetErrMsg = "No more socket descriptors are available.";
	break;
  case WSAEPROTONOSUPPORT:
	inetErrMsg = "The specified protocol is not supported.";
	break;
  case WSAEPROTOTYPE:
	inetErrMsg = "The specified protocol is the wrong type for this socket.";
	break;
  case WSAESOCKTNOSUPPORT:
	inetErrMsg = "The specified socket type is not supported in this address family.";
    break;
  default:
    inetErrMsg = "Unknown";
  };
#else
  inetErrNo = errno;
  inetErrMsg = strerror(inetErrNo);
#endif
  inetErrDesc = description;
}
