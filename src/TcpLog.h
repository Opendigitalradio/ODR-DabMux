/*
   Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Her Majesty the Queen in
   Right of Canada (Communications Research Center Canada)
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

#ifndef _TCP_LOG
#define _TCP_LOG

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#ifndef _WIN32
#  include <pthread.h>
#else
#  include <winsock2.h>
#  define pthread_t HANDLE
#endif

class TcpLog {
public:
    TcpLog();
    ~TcpLog();

    void open(const char *ident, const int option, const int port);
    void printHeader(const int priority, const char *format, ...);
    void print(const int priority, const char *format, ...);
    void close(void);

    const static int EMERG;     // system is unusable
    const static int ALERT;     // action must be taken immediately
    const static int CRIT;      // critical conditions
    const static int ERR;       // error conditions
    const static int WARNING;   // warning conditions
    const static int NOTICE;    // normal but significant condition
    const static int INFO;      // informational
    const static int DBG;       // debug-level messages

private:
    static void* listen(TcpLog* obj);

    int listenPort;
    int client;
    char* name;
    pthread_t serverThread;
    bool running;
    char* buffer;
    int bufferSize;
    char** headers;
};


#endif // _TCP_LOG
