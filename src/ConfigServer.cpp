/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2014
   Matthias P. Braendli, matthias.braendli@mpb.li
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

#include "ConfigServer.h"

#include <errno.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <limits>
#include <sstream>
#include <ctime>
#include <boost/thread.hpp>
#include "Log.h"

void ConfigServer::serverThread()
{
    m_fault = false;

    try {
        int accepted_sock;
        char buffer[256];
        char welcome_msg[256];
        struct sockaddr_in serv_addr, cli_addr;
        int n;

        int welcome_msg_len = snprintf(welcome_msg, 256,
                "%s %s Config Server\n",
                PACKAGE_NAME,
#if defined(GITVERSION)
                GITVERSION
#else
                PACKAGE_VERSION
#endif
                );


        m_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (m_sock < 0) {
            etiLog.level(error) << "Error opening Config Server socket: " <<
                strerror(errno);
            m_fault = true;
            return;
        }

        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = INADDR_ANY; // TODO listen only on 127.0.0.1
        serv_addr.sin_port = htons(m_listenport);

        if (bind(m_sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
            etiLog.level(error) << "Error binding Config Server socket: " <<
                strerror(errno);
            goto end_serverthread;
        }

        if (listen(m_sock, 5) < 0) {
            etiLog.level(error) << "Error listening on Config Server socket: " <<
                strerror(errno);
            goto end_serverthread;
        }

        m_running = true;

        while (m_running) {
            socklen_t cli_addr_len = sizeof(cli_addr);

            /* Accept actual connection from the client */
            accepted_sock = accept(m_sock,
                    (struct sockaddr *)&cli_addr,
                    &cli_addr_len);

            if (accepted_sock < 0) {
                etiLog.level(warn) << "Config Server cound not accept connection: " <<
                    strerror(errno);
                continue;
            }
            /* Send welcome message with version */
            n = write(accepted_sock, welcome_msg, welcome_msg_len);
            if (n < 0) {
                etiLog.level(warn) << "Error writing to Config Server socket " <<
                    strerror(errno);
                close(accepted_sock);
                continue;
            }

            /* receive command */
            memset(buffer, 0, 256);
            int n = read(accepted_sock, buffer, 255);
            if (n < 0) {
                etiLog.level(warn) << "Error reading from Config Server socket " <<
                    strerror(errno);
                close(accepted_sock);
                continue;
            }

            if (strcmp(buffer, "getconfig\n") == 0) {
                boost::unique_lock<boost::mutex> lock(m_mutex);
                m_pending_request = "getconfig";
                m_pending = true;

                while (m_pending) {
                    m_condition.wait(lock);
                }
                std::stringstream ss;
                boost::property_tree::info_parser::write_info(ss, m_pt);

                std::string response = ss.str();

                n = write(accepted_sock, response.c_str(), response.size());
            }
            else {
                int len = snprintf(buffer, 256, "Invalid command\n");
                n = write(accepted_sock, buffer, len);
            }

            if (n < 0) {
                etiLog.level(warn) << "Error writing to Config Server socket " <<
                    strerror(errno);
            }
            close(accepted_sock);
        }

end_serverthread:
        m_fault = true;
        close(m_sock);

    }
    catch (std::exception& e) {
        etiLog.level(error) << "Config server caught exception: " << e.what();
        m_fault = true;
    }
}

void ConfigServer::restart()
{
    m_restarter_thread = boost::thread(&ConfigServer::restart_thread,
            this, 0);
}

// This runs in a separate thread, because
// it would take too long to be done in the main loop
// thread.
void ConfigServer::restart_thread(long)
{
    m_running = false;

    if (m_listenport) {
        m_thread.interrupt();
        m_thread.join();
    }

    m_thread = boost::thread(&ConfigServer::serverThread, this);
}

