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

#ifndef _InetAddress
#define _InetAddress

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

// General libraries
#include <stdlib.h>
// Linux librairies
#ifndef _WIN32
// # include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <unistd.h>
# include <netdb.h>
# include <arpa/inet.h>
# include <pthread.h>
# define SOCKET                  int
# define INVALID_SOCKET  -1
# define closesocket             ::close
// Windows librairies
#else
#   include <winsock.h>
#   ifdef _MSC_VER
#       pragma comment(lib, "wsock32.lib")
#   elif defined(__BORLANDC__)
#       pragma(lib, "mswsock.lib")
#   endif
#   ifndef IN_MULTICAST
#       define IN_MULTICAST(a)  ((((unsigned long) (a)) & 0xf0000000) == 0xe0000000)
#   endif
#endif
// General definitions
#define INVALID_PORT            -1


/// The last error number
extern int inetErrNo;
/// The last error message
extern const char *inetErrMsg;
/// The description of the last error
extern const char *inetErrDesc;
/// Set the number, message and description of the last error
void setInetError(const char* description);


/**
 *  This class represents an Internet Protocol (IP) address.
 *  @author Pascal Charest pascal.charest@crc.ca
 */
class InetAddress {
 public:
  InetAddress(int port = 0, const char* name = NULL);
  InetAddress(const InetAddress &addr);
  ~InetAddress();

  sockaddr *getAddress();
  const char *getHostAddress();
  int getPort();
  int setAddress(const char *name);
  void setPort(int port);
  bool isMulticastAddress();

 private:
  sockaddr_in addr;
};


#endif
