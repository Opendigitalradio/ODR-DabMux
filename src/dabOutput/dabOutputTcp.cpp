/*
   Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Her Majesty the Queen in
   Right of Canada (Communications Research Center Canada)

   Copyright (C) 2013 Matthias P. Braendli
   http://mpb.li

   TCP output
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
#include <cstring>
#include <cstdio>
#include <signal.h>
#include <limits.h>
#include "dabOutput.h"
#include "TcpServer.h"

#ifdef _WIN32
#   include <io.h>
#   ifdef __MINGW32__
#       define FS_DECLARE_CFG_ARRAYS
#       include <winioctl.h>
#   endif
#   include <sdci.h>
#else
#   include <unistd.h>
#   include <sys/time.h>
#   ifndef O_BINARY
#       define O_BINARY 0
#   endif // O_BINARY
#endif

void* tcpThread(void* param)
{
    TcpSocket* client;

    DabOutputTcp* tcp = (DabOutputTcp*)param;

    while ((client = tcp->server->accept()) != NULL) {
        etiLog.log(info, "TCP server got a new client.\n");
        if (tcp->client != NULL) {
            delete tcp->client;
        }
        tcp->client = client;
    }
    etiLog.log(error, "TCP thread can't accept new client (%s)\n",
            inetErrDesc, inetErrMsg);

    return NULL;
}

int DabOutputTcp::Open(const char* name)
{
    char* hostport = strdup(name); // the name is actually an tuple host:port

    char* address;
    long port;
    address = strchr((char*)hostport, ':');
    if (address == NULL) {
        etiLog.log(error,
                "\"%s\" is an invalid format for tcp address: "
                "should be [address]:port - > aborting\n",
                hostport);
        goto tcp_open_fail;
    }

    // terminate string hostport after the host, and advance address to the port number
    *(address++) = 0;

    port = strtol(address, (char **)NULL, 10);
    if ((port == LONG_MIN) || (port == LONG_MAX)) {
        etiLog.log(error,
                "can't convert port number in tcp address %s\n", address);
        goto tcp_open_fail;
    }
    if (port == 0) {
        etiLog.log(error,
                "can't use port number 0 in tcp address\n");
        goto tcp_open_fail;
    }
    address = hostport;
    if (strlen(address) > 0) {
        if (this->server->create(port, address) == -1) {
            etiLog.log(error, "Can't create Tcp server on %s:%i "
                    "(%s: %s) -> aborting\n",
                    address, port, inetErrDesc, inetErrMsg);
            goto tcp_open_fail;
        }
    } else {
        if (this->server->create(port) == -1) {
            etiLog.log(error, "Can't create Tcp server on :%i "
                    "(%s: %s) -> aborting\n",
                    port, inetErrDesc, inetErrMsg);
            goto tcp_open_fail;
        }
    }

    //sprintf(name, "%s:%i", this->packet_->getAddress().getHostAddress(),
    //        this->packet_->getAddress().getPort());

    if (this->server->listen() == -1) {
        etiLog.log(error, "Can't listen on Tcp socket (%s: %s)\n",
                inetErrDesc, inetErrMsg);
        goto tcp_open_fail;
    }
#ifdef _WIN32
    this->thread_ = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)tcpThread, this, 0, NULL);
    if (this->thread_ == NULL) {
        fprintf(stderr, "Can't create TCP child");
        goto tcp_open_fail;
    }
#else
    if (pthread_create(&this->thread_, NULL, tcpThread, this)) {
        perror("Can't create TCP child");
        goto tcp_open_fail;
    }
#endif

    return 0;

tcp_open_fail:
    free(hostport);
    return -1;
}


int DabOutputTcp::Write(void* buffer, int size)
{

    if (this->client != NULL) {
        if (this->client->write(&size, 2) == 2) {
            if (this->client->write(buffer, size) != size) {
                return size;
            }
        }
        else {
            etiLog.log(info, "TCP server client disconnected.\n");
            delete this->client;
            this->client = NULL;
        }
    }
    return size;
}


int DabOutputTcp::Close()
{
    this->server->close();
    if( this->client != NULL )
        this->client->close();
#ifdef WIN32
    DWORD status;
    for (int i = 0; i < 5; ++i) {
        if (GetExitCodeThread(this->thread_, &status)) {
            break;
        }
        Sleep(100);
    }
    TerminateThread(this->thread_, 1);
#else
    pthread_kill(this->thread_, SIGPIPE);
#endif
    return 0;
}
