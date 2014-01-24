/*
   Copyright (C) 2009 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2014 Matthias P. Braendli
   http://mpb.li

   A TCP Socket server that serves statistics for monitoring purposes.

   This server is very easy to integrate with munin
        http://munin-monitoring.org/
   but is not specific to it.

   The TCP Server responds in JSON, and accepts two commands:
    - config
       -and-
    - values

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

#ifndef __STATS_SERVER_H
#define __STATS_SERVER_H

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <string>
#include <map>
#include <boost/thread.hpp>

#define MIN_FILL_BUFFER_UNDEF (-1)

struct InputStat
{
    InputStat() { reset(); }

    // minimum and maximum buffer fill since last reset
    long min_fill_buffer;
    long max_fill_buffer;

    // number of overruns and underruns since last reset
    long num_underruns;
    long num_overruns;

    void reset()
    {
        min_fill_buffer = MIN_FILL_BUFFER_UNDEF;
        max_fill_buffer = 0;

        num_underruns = 0;
        num_overruns = 0;
    }

    std::string encodeJSON();
};

class StatsServer
{
    public:
        StatsServer() : m_listenport(0), m_running(false) {}
        StatsServer(int listen_port) :
            m_listenport(listen_port),
            m_running(true),
            m_thread(&StatsServer::serverThread, this)
            {
                m_sock = 0;
            }

        ~StatsServer()
        {
            m_running = false;
            if (m_sock) {
                close(m_sock);
            }
            m_thread.join();
        }

        void registerInput(std::string id);
        // The input notifies the StatsServer about a new buffer size
        void notifyBuffer(std::string id, long bufsize);

        void notifyUnderrun(std::string id);
        void notifyOverrun(std::string id);

    private:
        /******* TCP Socket Server ******/
        // no copying (because of the thread)
        StatsServer(const StatsServer& other);

        void serverThread();

        int m_listenport;

        // serverThread runs in a separate thread
        bool m_running;
        boost::thread m_thread;

        int m_sock;

        /******* Statistics Data ********/
        std::map<std::string, InputStat> m_inputStats;

        /* Return a description of the configuration that will
         * allow to define what graphs to be created
         *
         * returns: a JSON encoded configuration
         */
        std::string getConfigJSON();

        /* Return the values for the statistics as defined in the configuration
         *
         * returns: JSON encoded statistics
         */
        std::string getValuesJSON();

        // mutex for accessing the map
        mutable boost::mutex m_mutex;
};


extern StatsServer* global_stats;

#endif

