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
#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <ctime>
#include <cmath>

#define MIN_FILL_BUFFER_UNDEF (-1)

/*** State handing ***/
/* An input can be in one of the following three states:
 */
enum input_state_t
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


/* The delay after which the glitch counter is reset */
#define INPUT_COUNTER_RESET_TIME 30 // minutes

/* How many glitches we tolerate in Streaming state before
 * we consider the input Unstable */
#define INPUT_UNSTABLE_THRESHOLD 3

/* For how long the input buffers must be empty before we move an input to the
 * NoData state.  */
#define INPUT_NODATA_TIMEOUT 30 // seconds

/* For silence detection, we count the number of occurrences the audio level
 * falls below a threshold.
 *
 * The counter is decreased for each frame that has good audio level.
 *
 * The counter saturates, and this value defines for how long the
 * input will be considered silent after a cut.
 *
 * If the count reaches a certain value, the input changes state
 * to Silence.  */
#define INPUT_AUDIO_LEVEL_THRESHOLD       -50  // dB
#define INPUT_AUDIO_LEVEL_SILENCE_COUNT    100 // superframes (120ms)
#define INPUT_AUDIO_LEVEL_COUNT_SATURATION 500 // superframes (120ms)

/* An example of how the state changes work.
 * The timeout is set to expire in 30 minutes
 * at each under-/overrun.
 *
 * The glitch counter is increased by one for each glitch (can be a
 * saturating counter), and set to zero when the counter timeout expires.
 *
 * The state is then simply depending on the glitch counter value.
 *
 * Graphical example:

 state     STREAMING      | UNSTABLE | STREAMING
 xruns         U     U    U
 glitch
  counter  0   1     2    3          0
 reset
  timeout  \   |\    |\   |\
            \  | \   | \  | \
             \ |  \  |  \ |  \
              \|   \ |   \|   \
               `    \|    `    \
                     `          \
                                 \
                                  \
                                   \
                                    \
  timeout expires ___________________\
                           <--30min-->
 */

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
        std::deque<long> buffer_fill_stats;
        std::chrono::time_point<std::chrono::steady_clock> time_last_buffer_notify;

        // counter of number of overruns and underruns since startup
        uint32_t num_underruns;
        uint32_t num_overruns;

        // Peak audio levels (linear 16-bit PCM) for the two channels.
        // Keep a FIFO of values from the last few seconds
        std::deque<int> peaks_left;
        std::deque<int> peaks_right;
        std::chrono::time_point<std::chrono::steady_clock> time_last_peak_notify;

        /************* STATE ***************/
        /* Variables used for determining the input state */
        int m_glitch_counter; // saturating counter
        int m_silence_counter; // saturating counter
        time_t m_time_last_event;

        // The mutex that has to be held during all notify and readout
        mutable boost::mutex m_mutex;
};

class ManagementServer
{
    public:
        ManagementServer() :
            m_zmq_context(),
            m_zmq_sock(m_zmq_context, ZMQ_REP),
            m_running(false),
            m_fault(false) { }

        ~ManagementServer()
        {
            m_running = false;
            m_fault = false;

            // TODO notify
            m_thread.join();
        }

        ManagementServer(const ManagementServer& other) = delete;
        ManagementServer& operator=(const ManagementServer& other) = delete;

        void open(int listenport)
        {
            m_listenport = listenport;
            if (m_listenport > 0) {
                m_thread = boost::thread(&ManagementServer::serverThread, this);
            }
        }

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

        bool fault_detected() { return m_fault; }
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
        boost::thread m_thread;
        boost::thread m_restarter_thread;

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
        boost::mutex m_statsmutex;

        /******** Configuration Data *******/
        boost::mutex m_configmutex;
        boost::property_tree::ptree m_pt;
};

// If necessary construct the management server singleton and return
// a reference to it
ManagementServer& get_mgmt_server();

