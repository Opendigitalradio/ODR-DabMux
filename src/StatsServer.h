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
#include <math.h>

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

/* For silence detection, we count the number of occurrences the audio level
 * falls below a threshold.
 *
 * The counter is decreased for each frame that has good audio level.
 *
 * The counter saturates, and this value defines for how long the
 * input will be considered silent after a cut.
 *
 * If the count reaches a certain value, the input changes state
 * to Silence.
 */
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
        InputStat(std::string name) : m_name(name)
        {
            /* Statistics */
            num_underruns = 0;
            num_overruns = 0;

            /* State handling */
            time_t now = time(NULL);
            m_time_last_event = now;
            m_time_last_buffer_nonempty = 0;
            m_buffer_empty = true;
            m_glitch_counter = 0;
            m_silence_counter = 0;

            reset();
        }

        void registerAtServer(void);

        ~InputStat();

        // Gets called each time the statistics are transmitted,
        // and resets the counters to zero
        void reset(void)
        {
            min_fill_buffer = MIN_FILL_BUFFER_UNDEF;
            max_fill_buffer = 0;

            peak_left = 0;
            peak_right = 0;
        }

        std::string& get_name(void) { return m_name; }

        /* This function is called for every frame read by
         * the multiplexer
         */
        void notifyBuffer(long bufsize)
        {
            boost::mutex::scoped_lock lock(m_mutex);

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
            boost::mutex::scoped_lock lock(m_mutex);

            // Statistics
            if (peak_left > this->peak_left) {
                this->peak_left = peak_left;
            }

            if (peak_right > this->peak_right) {
                this->peak_right = peak_right;
            }

            // State

            // using the smallest of the two channels
            // allows us to detect if only one channel
            // is silent.
            int minpeak = peak_left < peak_right ? peak_left : peak_right;

            const int16_t int16_max = std::numeric_limits<int16_t>::max();
            int peak_dB = minpeak ?
                round(20*log10((double)minpeak / int16_max)) :
                -90;

            if (peak_dB < INPUT_AUDIO_LEVEL_THRESHOLD) {
                if (m_silence_counter < INPUT_AUDIO_LEVEL_COUNT_SATURATION) {
                    m_silence_counter++;
                }
            }
            else {
                if (m_silence_counter > 0) {
                    m_silence_counter--;
                }
            }
        }

        void notifyUnderrun(void)
        {
            boost::mutex::scoped_lock lock(m_mutex);

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
            boost::mutex::scoped_lock lock(m_mutex);

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
        std::string m_name;

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
        int m_silence_counter; // saturating counter
        time_t m_time_last_event;
        time_t m_time_last_buffer_nonempty;
        bool m_buffer_empty;

        // The mutex that has to be held during all notify and readout
        mutable boost::mutex m_mutex;

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

        void registerInput(InputStat* is);
        void unregisterInput(std::string id);

        bool fault_detected() { return m_fault; }
        void restart(void);

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
        std::map<std::string, InputStat*> m_inputStats;

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

