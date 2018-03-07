/*
   Copyright (C) 2009 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2018
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://www.opendigitalradio.org

   A server that serves state information and statistics for
   monitoring purposes, and also serves the internal configuration
   property tree.

   This statistics server is very easy to integrate with munin
        http://munin-monitoring.org/
   but is not specific to it.

   The responds in JSON, and accepts the commands:
    - config
    - values
      Inspired by the munin equivalent, returns the configuration
      and the statistics values for every exported stat.

    - getptree
      Returns the internal boost property_tree that contains the
      multiplexer configuration DB.

   The server is using REQ/REP ZeroMQ sockets.
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

#pragma once

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include "zmq.hpp"
#include <string>
#include <map>
#include <atomic>
#include <chrono>
#include <deque>
#include <thread>
#include <mutex>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <cmath>

/*** State handing ***/
/* An input can be in one of the following three states:
 */
enum class input_state_t
{
    /* The input is waiting for data, all buffers are empty */
    NoData,
    /* The input is running, but has seen many underruns or overruns recently */
    Unstable,
    /* The input is running, but the audio level is too low, or has
     * been too low recently
     */
    Silence,
    /* The input is running stable */
    Streaming
};

/* InputStat takes care of
 * - saving the statistics for graphing
 * - calculating the state of the input for monitoring
 */
class InputStat
{
    public:
        InputStat(const std::string& name);
        InputStat(const InputStat& other) = delete;
        InputStat& operator=(const InputStat& other) = delete;
        ~InputStat();
        void registerAtServer(void);

        std::string get_name(void) const { return m_name; }

        /* This function is called for every frame read by
         * the multiplexer */
        void notifyBuffer(long bufsize);
        void notifyPeakLevels(int peak_left, int peak_right);
        void notifyUnderrun(void);
        void notifyOverrun(void);
        std::string encodeValuesJSON(void);
        input_state_t determineState(void);

    private:
        std::string m_name;

        /************ STATISTICS ***********/
        // Calculate minimum and maximum buffer fill from
        // a FIFO of values from the last few seconds
        std::deque<long> m_buffer_fill_stats;
        std::chrono::time_point<std::chrono::steady_clock> m_time_last_buffer_notify;

        // counter of number of overruns and underruns since startup
        uint32_t m_num_underruns;
        uint32_t m_num_overruns;

        // Peak audio levels (linear 16-bit PCM) for the two channels.
        // Keep a FIFO of values from the last minutes, apply
        // a short window to also see short-term fluctuations.
        std::deque<int> m_peaks_left;
        std::deque<int> m_peaks_right;
        std::chrono::time_point<std::chrono::steady_clock> m_time_last_peak_notify;

        size_t m_short_window_length = 0;

        /************* STATE ***************/
        /* Variables used for determining the input state */
        int m_glitch_counter; // saturating counter
        int m_silence_counter; // saturating counter
        std::chrono::time_point<std::chrono::steady_clock> m_time_last_event;

        // The mutex that has to be held during all notify and readout
        mutable std::mutex m_mutex;
};

class ManagementServer
{
    public:
        ManagementServer();
        ManagementServer(const ManagementServer& other) = delete;
        ManagementServer& operator=(const ManagementServer& other) = delete;
        ~ManagementServer();

        void open(int listenport);

        /* Un-/Register a statistics data source */
        void registerInput(InputStat* is);
        void unregisterInput(std::string id);

        /* Load a ptree given by the management server.
         *
         * Returns true if the ptree was updated
         */
        bool retrieve_new_ptree(boost::property_tree::ptree& pt);

        /* Update the copy of the configuration property tree and notify the
         * update to the internal server thread.
         */
        void update_ptree(const boost::property_tree::ptree& pt);

        bool fault_detected() const { return m_fault; }
        void restart(void);

    private:
        void restart_thread(long);

        /******* Server ******/
        zmq::context_t m_zmq_context;
        zmq::socket_t  m_zmq_sock;

        void serverThread(void);
        void handle_message(zmq::message_t& zmq_message);

        bool isInputRegistered(std::string& id);

        int m_listenport = 0;

        // serverThread runs in a separate thread
        std::atomic<bool> m_running;
        std::atomic<bool> m_fault;
        std::thread m_thread;
        std::thread m_restarter_thread;

        /******* Statistics Data ********/
        std::map<std::string, InputStat*> m_inputStats;

        /* Return a description of the configuration that will
         * allow to define what graphs to be created
         *
         * returns: a JSON encoded configuration
         */
        std::string getStatConfigJSON();

        /* Return the values for the statistics as defined in the configuration
         *
         * returns: JSON encoded statistics
         */
        std::string getValuesJSON();

        // mutex for accessing the map
        std::mutex m_statsmutex;

        /******** Configuration Data *******/
        std::mutex m_configmutex;
        boost::property_tree::ptree m_pt;
};

// If necessary construct the management server singleton and return
// a reference to it
ManagementServer& get_mgmt_server();

