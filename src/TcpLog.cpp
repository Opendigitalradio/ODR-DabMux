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

#include "TcpLog.h"
#include "InetAddress.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#ifdef _WIN32
#   include <io.h>
#
#   define vsnprintf _vsnprintf
#   define MSG_NOSIGNAL 0
#   define socklen_t int
#else
#   include <sys/socket.h>
#   include <netinet/in.h>
#   include <arpa/inet.h>
#   include <unistd.h>
#endif

#include <sstream>


const int TcpLog::EMERG   = 0;      // system is unusable
const int TcpLog::ALERT   = 1;      // action must be taken immediately
const int TcpLog::CRIT    = 2;      // critical conditions
const int TcpLog::ERR     = 3;      // error conditions
const int TcpLog::WARNING = 4;      // warning conditions
const int TcpLog::NOTICE  = 5;      // normal but significant condition
const int TcpLog::INFO    = 6;      // informational
const int TcpLog::DBG     = 7;      // debug-level messages


TcpLog::TcpLog() : listenPort(0), client(-1), name(NULL), serverThread(0), running(false)
{
    buffer = (char*)malloc(512);
    bufferSize = 512;
    headers = (char**)malloc(sizeof(char**));
    headers[0] = NULL;
}


TcpLog::~TcpLog()
{
    if (running) {
        running = false;
    }
    if (client != -1) {
        ::close(client);
    }
    if (headers != NULL) {
        for (int count = 0; headers[count] != NULL; ++count) {
            free(headers[count]);
        }
        free(headers);
    }
}


//TcpLog::TcpLog(int port);


void TcpLog::open(const char *ident, const int option, const int port)
{
    listenPort = port;

    if (name != NULL) {
        free(name);
    }
    name = strdup(ident);

    if (running) {
        running = false;
#ifdef WIN32
        DWORD status;
        for (int i = 0; i < 5; ++i) {
            if (GetExitCodeThread(serverThread, &status)) {
                break;
            }
            Sleep(100);
        }
        TerminateThread(serverThread, 1);
#else
        pthread_join(serverThread, NULL);
#endif
    }
    running = true;
#ifdef _WIN32
    serverThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)listen, this, 0, NULL);
    if (serverThread == NULL) {
        fprintf(stderr, "Can't create TCP listen thread\n");
    }
#else
    pthread_create(&serverThread, NULL, (void*(*)(void*))listen, this);
#endif
}


void TcpLog::printHeader(const int priority, const char *format, ...)
{
    static char last = '\n';
    std::string beginning;
    std::string message;
    int ret;


    beginning = "<";
    beginning += '0' + (priority % 10);
    beginning += "> ";

    va_list argp;
    va_start(argp, format);
    for (ret = vsnprintf(buffer, bufferSize, format, argp);
            ret >= bufferSize || ret == -1;
            ret = vsprintf(buffer, format, argp)) {
        if (ret == -1) {
            buffer = (char*)realloc(buffer, bufferSize * 2);
            bufferSize *= 2;
        } else {
            buffer = (char*)realloc(buffer, ret + 1);
            bufferSize = ret + 1;
        }
    }
    va_end(argp);

    if (last == '\n') {
        message += beginning;
    }
    message += buffer;

    last = buffer[ret - 1];

    for (unsigned long pos = message.find('\n');
            pos != std::string::npos && ++pos != message.size();
            pos = message.find('\n', pos)) {
        message.insert(pos, beginning);
    }

    fprintf(stderr, message.c_str());
    if (client != -1) {
        if (write(client, message.c_str(), message.size())
                != (int)message.size()) {
            fprintf(stderr, "<6> Client closed\n");
            ::close(client);
            client = -1;
        }
    }

    int count = 0;
    while (headers[count++] != NULL) {
    }
    headers = (char**)realloc(headers, sizeof(char**) * (count + 1));
    headers[count - 1] = strdup(message.c_str());
    headers[count] = NULL;
}


void TcpLog::print(const int priority, const char *format, ...)
{
    static char last = '\n';
    std::string beginning;
    std::string message;
    int ret;


    beginning = "<";
    beginning += '0' + (priority % 10);
    beginning += "> ";

    va_list argp;
    va_start(argp, format);
    for (ret = vsnprintf(buffer, bufferSize, format, argp);
            ret >= bufferSize || ret == -1;
            ret = vsprintf(buffer, format, argp)) {
        if (ret == -1) {
            buffer = (char*)realloc(buffer, bufferSize * 2);
            bufferSize *= 2;
        } else {
            buffer = (char*)realloc(buffer, ret + 1);
            bufferSize = ret + 1;
        }
    }
    va_end(argp);

    if (last == '\n') {
        message += beginning;
    }
    message += buffer;

    last = buffer[ret - 1];

    for (unsigned long pos = message.find('\n');
            pos != std::string::npos && ++pos != message.size();
            pos = message.find('\n', pos)) {
        message.insert(pos, beginning);
    }

    fprintf(stderr, message.c_str());
    if (client != -1) {
        if (send(client, message.c_str(), message.size(), MSG_NOSIGNAL)
                != (int)message.size()) {
            fprintf(stderr, "<6> Client closed\n");
            ::close(client);
            client = -1;
        }
    }
}


void TcpLog::close(void)
{
}


void* TcpLog::listen(TcpLog* obj)
{
    int server;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    socklen_t addrlen = sizeof(client_addr);

#ifdef _WIN32
    WSADATA wsaData;
    WORD wVersionRequested = wVersionRequested = MAKEWORD( 2, 2 );

    int res = WSAStartup( wVersionRequested, &wsaData );
    if (res) {
        fprintf(stderr, "Can't initialize winsock\n");
        ExitThread(1);
    }
#endif
    server = socket(PF_INET, SOCK_STREAM, 0);
    if (server == INVALID_SOCKET) {
#ifdef _WIN32
        fprintf(stderr, "Can't create socket\n");
        ExitThread(1);
#else
        perror("Can't create socket");
        pthread_exit(NULL);
#endif
    }

    int reuse = 1;
    if (setsockopt(server, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse))
            == -1) {
        perror("Can't set socket reusable");
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(obj->listenPort);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server, (struct sockaddr*)&server_addr, sizeof(server_addr))
            == -1) {
        perror("Can't bind socket");
        fprintf(stderr, " port: %i\n", obj->listenPort);
#ifdef _WIN32
        ExitThread(1);
#else
        pthread_exit(NULL);
#endif
    }

    if (::listen(server, 1) == -1) {
        perror("Can't listen on socket");
#ifdef _WIN32
        ExitThread(1);
#else
        pthread_exit(NULL);
#endif
    }

    while (obj->running) {
        int client = accept(server, (struct sockaddr*)&client_addr, &addrlen);
        if (client == INVALID_SOCKET) {
#ifdef _WIN32
            if (WSAGetLastError() != WSAEINTR) {
                setInetError("Can't accept client");
                fprintf(stderr, "%s: %s\n", inetErrDesc, inetErrMsg);
                ExitThread(1);
            }
#else
            perror("Can't accept client");
            pthread_exit(NULL);
#endif
        }
        obj->print(INFO, "%s:%d connected\n",
                inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        if (obj->client != -1) {
            ::close(obj->client);
        }
        obj->client = client;
        for (int i = 0; obj->headers[i] != NULL; ++i) {
            send(client, obj->headers[i], strlen(obj->headers[i]), MSG_NOSIGNAL);
        }
    }
    if (obj->client != -1) {
        ::close(obj->client);
        obj->client = -1;
    }
    ::close(server);

#ifdef _WIN32
    ExitThread(1);
#else
    pthread_exit(NULL);
#endif
    return NULL;
}
