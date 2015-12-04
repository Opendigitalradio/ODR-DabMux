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

#include "TcpSocket.h"
#include <iostream>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#ifdef _WIN32
#else
#   include <unistd.h>
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
int TcpSocket::init()
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
int TcpSocket::clean()
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
TcpSocket::TcpSocket() :
  listenSocket(INVALID_SOCKET)
{
  TRACE_CLASS("TcpSocket", "TcpSocket()");
}


/**
 *  One step constructor.
 *  @param port The port number on which the socket will be bind
 *  @param name The IP address on which the socket will be bind.
 *              It is used to bind the socket on a specific interface if
 *              the computer have many NICs.
 */
TcpSocket::TcpSocket(int port, char *name) :
  listenSocket(INVALID_SOCKET)
{
  TRACE_CLASS("TcpSocket", "TcpSocket(int, char*)");
  create(port, name);
}


/**
 *  Close the underlying socket.
 *  @return 0  if ok
 *          -1 if error
 */
int TcpSocket::close()
{
    TRACE_CLASS("TcpSocket", "close()");
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
int TcpSocket::create(int port, char *name)
{
  TRACE_CLASS("TcpSocket", "create(int, char*)");
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


/// Destructor
TcpSocket::~TcpSocket() {
  TRACE_CLASS("TcpSocket", "~TcpSocket()");
  close();
}


/**
 *  Receive an telnet packet, i.e a TCP stream ending with carriage return.
 *  @param data The buffer that will receive data.
 *  @param size The buffer size.
 *  @return > 0 if ok, -1 (SOCKET_ERROR) if error
 */
int TcpSocket::telnetRead(void* data, int size)
{
  TRACE_CLASS("TcpSocket", "read(void*, size)");
  int ret;
  
  printf("selectCall\n");
  
  printf("readread 1\n");  
  char *line=GetLine(listenSocket);
  ret=strlen(line);
  printf("readread 2\n");  
  if (ret <= size)
  {
	strcpy((char*)data, line);	
  }
  else
  {
//	size_t n = size;
	strcpy((char*)data, line);
	ret = size;
  }
  printf("TELNET READ returned %d\n", ret);
  return ret;  
}

/**
 *  Receive a TCP stream.
 *  @param data The buffer that will receive data.
 *  @param size The buffer size.
 *  @return > 0 if ok, -1 (SOCKET_ERROR) if error
 */
int TcpSocket::read(void* data, int size)
{
  TRACE_CLASS("TcpSocket", "read(void*, size)");
 
  int ret = recv(listenSocket, (char*)data, size, 0);
  if (ret == SOCKET_ERROR) {
    setInetError("Can't receive TCP packet");
    return -1;
  }
  return ret;  
}


#define MAX 512
char* TcpSocket::GetLine(int fd)
{
  static char line[MAX];
  static char netread[MAX] = "";
  int n, len;
  char *p;

  len = strlen(netread);

  /* look for \r\n in netread buffer */
  p = strstr(netread, "\r\n");
  if (p == NULL) {  
    /* fill buff - no \r\n found */
    //n = ::read(fd, (void*)(netread+len), (size_t)(MAX-len));
    n = recv(fd, (netread+len), MAX-len, 0);
    if (n == SOCKET_ERROR) {
      setInetError("Can't receive TCP packet");
      return NULL;
    }
    len += n;
    netread[len] = '\0';
    if (n>0)
	    return GetLine(fd);
  }
  if (p!=NULL)
  {
	  *p = '\0';
	  strcpy(line, netread);
	  /* copy rest of buf down */
	  memmove(netread, p+2, strlen(p+2)+1);  
  }
  return line;
}


/**
 *  Send an TCP packet.
 *  @param data The buffer taht will be sent.
 *  @param size Number of bytes to send.
 *  return 0 if ok, -1 if error
 */
int TcpSocket::write(const void* data, int size)
{
#ifdef DUMP
  TRACE_CLASS("TcpSocket", "write(const void*, int)");
#endif

  // ignore BROKENPIPE signal (we handle it instead)
//  void* old_sigpipe = signal ( SIGPIPE, SIG_IGN );
  // try to send data
  int ret = send(listenSocket, (const char*)data, size, 0 /*MSG_NOSIGNAL*/	);
  // restore the BROKENPIPE handling
//  signal ( SIGPIPE,  (__sighandler_t)old_sigpipe );
  if (ret == SOCKET_ERROR) {
    setInetError("Can't send TCP packet");
    return -1;
  }
  return ret;
}


int TcpSocket::setDestination(InetAddress &addr)
{
  address = addr;
  int ret = connect(listenSocket, addr.getAddress(), sizeof(*addr.getAddress()));
  // A etre verifier: code de retour differend entre Linux et Windows
  return ret;
}


void TcpSocket::setSocket(SOCKET socket, InetAddress &addr)
{
  if (listenSocket != INVALID_SOCKET)
    closesocket(listenSocket);
  listenSocket = socket;
  address = addr;
}


InetAddress TcpSocket::getAddress()
{
  return address;
}


int TcpSocket::PeekCount()
{
	int count;
	char tempBuffer[3];
	int size=3;
	
	count = recv(listenSocket, tempBuffer, size, MSG_PEEK);
	if (count == -1)
	{
		printf("ERROR WHEN PEEKING SOCKET\n");
	}		
	return count;
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
