/*
   Copyright (C) 2009 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2014 Matthias P. Braendli
   http://mpb.li

   A TCP Socket server that serves state information and statistics for
   monitoring purposes.

   This server is very easy to integrate with munin
        http://munin-monitoring.org/
   but is not specific to it.

   The TCP Server responds in JSON, and accepts two commands:
    - config
    - values
      Inspired by the munin equivalent

    - state
      Returns the state of each input

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
#include <ctime>

#define MIN_FILL_BUFFER_UNDEF (-1)

/*** State handing ***/
/* An input can be in one of the following three states:
 */
enum input_state_t
{
    /* The input is waiting for data, all buffers are empty */
    NoData,

    /* The input is running, but has seen many underruns or overruns */
    Unstable,

    /* The input is running stable */
    Streaming
};


/* The delay after which the glitch counter is reset
 */
#define INPUT_COUNTER_RESET_TIME 30 // minutes

/* How many glitches we tolerate in Streaming state before
 * we consider the input Unstable
 */
#define INPUT_UNSTABLE_THRESHOLD 3

/* For how long the input buffers must be empty before we move an input to the
 * NoData state.
 */
#define INPUT_NODATA_TIMEOUT 30 // seconds

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
        InputStat() {
            /* Statistics */
            num_underruns = 0;
            num_overruns = 0;

            /* State handling */
            time_t now = time(NULL);
            m_time_last_event = now;
            m_time_last_buffer_nonempty = 0;
            m_buffer_empty = true;
            m_glitch_counter = 0;

            reset();
        }


        // Gets called each time the statistics are transmitted,
        // and resets the counters to zero
        void reset(void)
        {
            min_fill_buffer = MIN_FILL_BUFFER_UNDEF;
            max_fill_buffer = 0;

            peak_left = 0;
            peak_right = 0;
        }

        /* This function is called for every frame read by
         * the multiplexer
         */
        void notifyBuffer(long bufsize)
        {
            // Statistics
            if (bufsize > max_fill_buffer) {
                max_fill_buffer = bufsize;
            }

            if (bufsize < min_fill_buffer ||
                    min_fill_buffer == MIN_FILL_BUFFER_UNDEF) {
                min_fill_buffer = bufsize;
            }

            // State
            m_buffer_empty = (bufsize == 0);
            if (!m_buffer_empty) {
                m_time_last_buffer_nonempty = time(NULL);
            }
        }

        void notifyPeakLevels(int peak_left, int peak_right)
        {
            // Statistics
            if (peak_left > this->peak_left) {
                this->peak_left = peak_left;
            }

            if (peak_right > this->peak_right) {
                this->peak_right = peak_right;
            }

            // State
            // TODO add silence detection
        }

        void notifyUnderrun(void)
        {
            // Statistics
            num_underruns++;

            // State
            m_time_last_event = time(NULL);
            if (m_glitch_counter < INPUT_UNSTABLE_THRESHOLD) {
                m_glitch_counter++;
            }
        }

        void notifyOverrun(void)
        {
            // Statistics
            num_overruns++;

            // State
            m_time_last_event = time(NULL);
            if (m_glitch_counter < INPUT_UNSTABLE_THRESHOLD) {
                m_glitch_counter++;
            }
        }

        std::string encodeValuesJSON(void);

        std::string encodeStateJSON(void);

        input_state_t determineState(void);

    private:
        /************ STATISTICS ***********/
        // minimum and maximum buffer fill since last reset
        long min_fill_buffer;
        long max_fill_buffer;

        // counter of number of overruns and underruns since startup
        uint32_t num_underruns;
        uint32_t num_overruns;

        // peak audio levels (linear 16-bit PCM) for the two channels
        int peak_left;
        int peak_right;

        /************* STATE ***************/
        /* Variables used for determining the input state */
        int m_glitch_counter; // saturating counter
        time_t m_time_last_event;
        time_t m_time_last_buffer_nonempty;
        bool m_buffer_empty;

};

class StatsServer
{
    public:
        StatsServer() :
            m_listenport(0),
            m_running(false),
            m_fault(false)
            { }

        StatsServer(int listen_port) :
            m_listenport(listen_port),
            m_running(false),
            m_fault(false),
            m_thread(&StatsServer::serverThread, this)
            {
                m_sock = 0;
            }

        ~StatsServer()
        {
            m_running = false;
            m_fault = false;
            if (m_sock) {
                close(m_sock);
            }
            m_thread.join();
        }

        void registerInput(std::string id);

        bool fault_detected() { return m_fault; }
        void restart(void);

        /* The following notify functions are used by the input to
         * inform the StatsServer about new values
         */
        void notifyBuffer(std::string id, long bufsize);
        void notifyPeakLevels(std::string id, int peak_left, int peak_right);
        void notifyUnderrun(std::string id);
        void notifyOverrun(std::string id);

    private:
        void restart_thread(long);

        /******* TCP Socket Server ******/
        // no copying (because of the thread)
        StatsServer(const StatsServer& other);

        void serverThread(void);

        bool isInputRegistered(std::string& id);

        int m_listenport;

        // serverThread runs in a separate thread
        bool m_running;
        bool m_fault;
        boost::thread m_thread;
        boost::thread m_restarter_thread;

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

        /* Return the state of each input
         *
         * returns: JSON encoded state
         */
        std::string getStateJSON();

        // mutex for accessing the map
        mutable boost::mutex m_mutex;
};


extern StatsServer* global_stats;

#endif

