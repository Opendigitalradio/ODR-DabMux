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

#ifndef __CONFIG_SERVER_H_
#define __CONFIG_SERVER_H_

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/info_parser.hpp>
#include <boost/thread.hpp>
#include <cstdio>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>

class ConfigServer
{
    public:
        ConfigServer() :
            m_listenport(0),
            m_running(false),
            m_fault(false)
            { }

        ConfigServer(int listen_port) :
            m_listenport(listen_port),
            m_running(false),
            m_fault(false),
            m_thread(&ConfigServer::serverThread, this)
            {
                m_sock = 0;
            }

        ~ConfigServer()
        {
            m_running = false;
            m_fault = false;
            if (m_sock) {
                close(m_sock);
            }
            m_thread.join();
        }

        bool request_pending() {
            return m_pending;
        }

        void update_ptree(const boost::property_tree::ptree& pt) {
            boost::unique_lock<boost::mutex> lock(m_mutex);
            m_pt = pt;
            m_pending = false;

            m_condition.notify_one();
        }

        bool fault_detected() { return m_fault; }
        void restart(void);

    private:
        void restart_thread(long);

        /******* TCP Socket Server ******/
        // no copying (because of the thread)
        ConfigServer(const ConfigServer& other);

        void serverThread(void);

        int m_listenport;

        // serverThread runs in a separate thread
        bool m_running;
        bool m_fault;
        boost::thread m_thread;
        boost::thread m_restarter_thread;

        int m_sock;

        bool m_pending;
        std::string m_pending_request;
        boost::condition_variable m_condition;
        boost::mutex m_mutex;

        boost::property_tree::ptree m_pt;
};


#endif

