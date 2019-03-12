/*
   Copyright (C) 2009 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2018
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://www.opendigitalradio.org

   A TCP Socket server that serves state information and statistics for
   monitoring purposes, and also serves the internal configuration
   property tree.
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

#include <errno.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <limits>
#include <sstream>
#include <algorithm>
#include <boost/version.hpp>
#include "ManagementServer.h"
#include "Log.h"

using namespace std;

#define MIN_FILL_BUFFER_UNDEF (-1)

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

/* The delay after which the glitch counter is reset */
static constexpr auto
INPUT_COUNTER_RESET_TIME = std::chrono::minutes(30);

/* How many glitches we tolerate in Streaming state before
 * we consider the input Unstable */
static constexpr int
INPUT_UNSTABLE_THRESHOLD = 3;

/* For how long the input buffers must be empty before we move an input to the
 * NoData state.  */
static constexpr auto
INPUT_NODATA_TIMEOUT  = std::chrono::seconds(30);

/* Keep 30s of min/max buffer fill information so that we can catch meaningful
 * values even if we have a slow poller */
static constexpr auto
BUFFER_STATS_KEEP_DURATION = std::chrono::seconds(30);

/* Audio level information changes faster than buffer levels, so it makes sense
 * to poll much faster. If we take the peak over too much data, we will hide
 * the interesting short-time fluctuations. At the same time, we want to have a
 * statistic that also catches the rare peaks, for slow pollers. */
static constexpr auto
PEAK_STATS_SHORT_WINDOW = std::chrono::milliseconds(500);
static constexpr auto
PEAK_STATS_KEEP_DURATION = std::chrono::minutes(5);

ManagementServer& get_mgmt_server()
{
    static ManagementServer mgmt_server;

    return mgmt_server;

    /* Warning, do not use the mgmt_server in the destructor
     * of another global object: you don't know which one
     * gets destroyed first
     */
}

void ManagementServer::registerInput(InputStat* is)
{
    unique_lock<mutex> lock(m_statsmutex);

    std::string id(is->get_name());

    if (m_inputStats.count(id) == 1) {
        etiLog.level(error) <<
            "Double registration in MGMT Server with id '" <<
            id << "'";
        return;
    }

    m_inputStats[id] = is;
}

void ManagementServer::unregisterInput(std::string id)
{
    unique_lock<mutex> lock(m_statsmutex);

    if (m_inputStats.count(id) == 1) {
        m_inputStats.erase(id);
    }
}


bool ManagementServer::isInputRegistered(std::string& id)
{
    unique_lock<mutex> lock(m_statsmutex);

    if (m_inputStats.count(id) == 0) {
        etiLog.level(error) <<
            "Management Server: id '" <<
            id << "' does was not registered";
        return false;
    }
    return true;
}

std::string ManagementServer::getStatConfigJSON()
{
    unique_lock<mutex> lock(m_statsmutex);

    std::ostringstream ss;
    ss << "{ \"config\" : [\n";

    std::map<std::string,InputStat*>::iterator iter;
    int i = 0;
    for(iter = m_inputStats.begin(); iter != m_inputStats.end();
            ++iter, i++)
    {
        std::string id = iter->first;

        if (i > 0) {
            ss << ", ";
        }

        ss << " \"" << id << "\" ";
    }

    ss << "] }\n";

    return ss.str();
}

std::string ManagementServer::getValuesJSON()
{
    unique_lock<mutex> lock(m_statsmutex);

    std::ostringstream ss;
    ss << "{ \"values\" : {\n";

    std::map<std::string,InputStat*>::iterator iter;
    int i = 0;
    for(iter = m_inputStats.begin(); iter != m_inputStats.end();
            ++iter, i++)
    {
        const std::string& id = iter->first;
        InputStat* stats = iter->second;

        if (i > 0) {
            ss << " ,\n";
        }

        ss << " \"" << id << "\" : ";
        ss << stats->encodeValuesJSON();
    }

    ss << "}\n}\n";

    return ss.str();
}

ManagementServer::ManagementServer() :
    m_zmq_context(),
    m_zmq_sock(m_zmq_context, ZMQ_REP),
    m_running(false),
    m_fault(false)
{ }

ManagementServer::~ManagementServer()
{
    m_running = false;
    if (m_thread.joinable()) {
        m_thread.join();
        m_fault = false;
    }
}

void ManagementServer::open(int listenport)
{
    m_listenport = listenport;
    if (m_listenport > 0) {
        m_thread = std::thread(&ManagementServer::serverThread, this);
    }
}

void ManagementServer::restart()
{
    m_restarter_thread = thread(&ManagementServer::restart_thread, this, 0);
}

// This runs in a separate thread, because
// it would take too long to be done in the main loop
// thread.
void ManagementServer::restart_thread(long)
{
    m_running = false;

    if (m_thread.joinable()) {
        m_thread.join();
        m_fault = false;
    }

    m_thread = thread(&ManagementServer::serverThread, this);
}

void ManagementServer::serverThread()
{
    m_running = true;
    m_fault = false;

    try {
        std::string bind_addr = "tcp://127.0.0.1:" + to_string(m_listenport);
        m_zmq_sock.bind(bind_addr.c_str());

        zmq::pollitem_t pollItems[] = { {m_zmq_sock, 0, ZMQ_POLLIN, 0} };

        while (m_running) {
            zmq::poll(pollItems, 1, 1000);

            if (pollItems[0].revents & ZMQ_POLLIN) {
                zmq::message_t zmq_message;
                m_zmq_sock.recv(&zmq_message);
                handle_message(zmq_message);
            }
        }
    }
    catch (const exception &e) {
        etiLog.level(error) << "Exception in ManagementServer: " <<
            e.what();
    }
    m_fault = true;
}

void ManagementServer::handle_message(zmq::message_t& zmq_message)
{
    std::stringstream answer;
    std::string data((char*)zmq_message.data(), zmq_message.size());

    try {
        if (data == "info") {

            answer <<
                "{ " <<
                "\"service\": \"" <<
                PACKAGE_NAME << " " <<
#if defined(GITVERSION)
                GITVERSION <<
#else
                PACKAGE_VERSION <<
#endif
                " MGMT Server\", "
                <<
                "\"version\": \"" <<
#if defined(GITVERSION)
                GITVERSION <<
#else
                PACKAGE_VERSION <<
#endif
                "\" "
                << "}\n";
        }
        else if (data == "config") {
            answer << getStatConfigJSON();
        }
        else if (data == "values") {
            answer << getValuesJSON();
        }
        else if (data == "getptree") {
            unique_lock<mutex> lock(m_configmutex);
            boost::property_tree::json_parser::write_json(answer, m_pt);
        }
        else {
            etiLog.level(warn) << "ManagementServer: Invalid request '" << data << "'";
            answer << "Invalid command";
        }

        std::string answerstr(answer.str());
        m_zmq_sock.send(answerstr.c_str(), answerstr.size());
    }
    catch (const std::exception& e) {
        etiLog.level(error) <<
            "MGMT server caught exception: " <<
            e.what();
    }
}

void ManagementServer::update_ptree(const boost::property_tree::ptree& pt)
{
    if (m_running) {
        unique_lock<mutex> lock(m_configmutex);
        m_pt = pt;
    }
}

/************************************************/

InputStat::InputStat(const std::string& name) :
    m_name(name),
    m_time_last_event(std::chrono::steady_clock::now())
{
}

InputStat::~InputStat()
{
    get_mgmt_server().unregisterInput(m_name);
}

void InputStat::registerAtServer()
{
    get_mgmt_server().registerInput(this);
}

void InputStat::notifyBuffer(long bufsize)
{
    unique_lock<mutex> lock(m_mutex);

    using namespace std::chrono;
    const auto time_now = steady_clock::now();
    m_buffer_fill_stats.push_front({time_now, bufsize});

    // Keep only stats whose timestamp are more recent than
    // BUFFER_STATS_KEEP_DURATION ago
    m_buffer_fill_stats.erase(remove_if(
                m_buffer_fill_stats.begin(), m_buffer_fill_stats.end(),
                [&](const fill_stat_t& fs) {
                    return fs.timestamp + BUFFER_STATS_KEEP_DURATION < time_now;
                }),
                m_buffer_fill_stats.end());
}

void InputStat::notifyPeakLevels(int peak_left, int peak_right)
{
    unique_lock<mutex> lock(m_mutex);

    using namespace std::chrono;
    const auto time_now = steady_clock::now();

    m_peak_stats.push_front({time_now, peak_left, peak_right});

    // Keep only stats whose timestamp are more recent than
    // BUFFER_STATS_KEEP_DURATION ago
    m_peak_stats.erase(remove_if(
                m_peak_stats.begin(), m_peak_stats.end(),
                [&](const peak_stat_t& ps) {
                    return ps.timestamp + PEAK_STATS_KEEP_DURATION < time_now;
                }),
                m_peak_stats.end());

    if (m_peak_stats.size() >= 2) {
        // Calculate the peak over the short window
        vector<peak_stat_t> short_peaks;
        copy_if(m_peak_stats.begin(), m_peak_stats.end(),
                back_inserter(short_peaks),
                [&](const peak_stat_t& ps) {
                    return ps.timestamp + PEAK_STATS_SHORT_WINDOW >= time_now;
                });

        const auto& short_left_peak_max = max_element(
                short_peaks.begin(), short_peaks.end(),
                [](const peak_stat_t& lhs, const peak_stat_t& rhs) {
                    return lhs.peak_left < rhs.peak_left;
                });

        const auto& short_right_peak_max = max_element(
                short_peaks.begin(), short_peaks.end(),
                [](const peak_stat_t& lhs, const peak_stat_t& rhs) {
                    return lhs.peak_right < rhs.peak_right;
                });

        // Using the lower of the two channels allows us to detect if only one
        // channel is silent.
        const int lower_peak = min(
                short_left_peak_max->peak_left, short_right_peak_max->peak_right);

        // State
        const int16_t int16_max = std::numeric_limits<int16_t>::max();
        int peak_dB = lower_peak ?
            round(20*log10((double)lower_peak / int16_max)) :
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
}

void InputStat::notifyUnderrun(void)
{
    unique_lock<mutex> lock(m_mutex);

    // Statistics
    m_num_underruns++;

    // State
    m_time_last_event = std::chrono::steady_clock::now();
    if (m_glitch_counter < INPUT_UNSTABLE_THRESHOLD) {
        m_glitch_counter++;
    }
    else {
        // As we don't receive level notifications anymore, clear the
        // audio level information
        m_peak_stats.clear();
    }
}

void InputStat::notifyOverrun(void)
{
    unique_lock<mutex> lock(m_mutex);

    // Statistics
    m_num_overruns++;

    // State
    m_time_last_event = std::chrono::steady_clock::now();
    if (m_glitch_counter < INPUT_UNSTABLE_THRESHOLD) {
        m_glitch_counter++;
    }
}

std::string InputStat::encodeValuesJSON()
{
    std::ostringstream ss;

    const int16_t int16_max = std::numeric_limits<int16_t>::max();

    unique_lock<mutex> lock(m_mutex);

    int peak_left_short = 0;
    int peak_right_short = 0;
    int peak_left = 0;
    int peak_right = 0;

    if (not m_peak_stats.empty()) {
        peak_left = max_element(m_peak_stats.begin(), m_peak_stats.end(),
                [](const peak_stat_t& lhs, const peak_stat_t& rhs) {
                    return lhs.peak_left < rhs.peak_left;
                })->peak_left;
        peak_right = max_element(m_peak_stats.begin(), m_peak_stats.end(),
                [](const peak_stat_t& lhs, const peak_stat_t& rhs) {
                    return lhs.peak_right < rhs.peak_right;
                })->peak_right;

        if (m_peak_stats.size() > m_short_window_length) {
            peak_left_short = max_element(m_peak_stats.begin(),
                    m_peak_stats.begin() + m_short_window_length,
                [](const peak_stat_t& lhs, const peak_stat_t& rhs) {
                    return lhs.peak_left < rhs.peak_left;
                })->peak_left;

            peak_right_short = max_element(m_peak_stats.begin(),
                    m_peak_stats.begin() + m_short_window_length,
                [](const peak_stat_t& lhs, const peak_stat_t& rhs) {
                    return lhs.peak_right < rhs.peak_right;
                })->peak_right;
        }
        else {
            peak_left_short = peak_left;
            peak_right_short = peak_right;
        }
    }

    long min_fill_buffer = MIN_FILL_BUFFER_UNDEF;
    long max_fill_buffer = 0;
    if (not m_buffer_fill_stats.empty()) {
        const auto& buffer_min_max_fill = minmax_element(
                m_buffer_fill_stats.begin(), m_buffer_fill_stats.end(),
                [](const fill_stat_t& lhs, const fill_stat_t& rhs) {
                    return lhs.bufsize < rhs.bufsize;
                });
        min_fill_buffer = buffer_min_max_fill.first->bufsize;
        max_fill_buffer = buffer_min_max_fill.second->bufsize;
    }

    /* convert to dB */
    auto to_dB = [](int p) {
        int dB = -90;
        if (p) {
            dB = round(20*log10((double)p / int16_max));
        }
        return dB;
    };

    ss <<
    "{ \"inputstat\" : {"
        "\"min_fill\": " << min_fill_buffer << ", "
        "\"max_fill\": " << max_fill_buffer << ", "
        "\"peak_left\": " << to_dB(peak_left_short) << ", "
        "\"peak_right\": " << to_dB(peak_right_short) << ", "
        "\"peak_left_slow\": " << to_dB(peak_left) << ", "
        "\"peak_right_slow\": " << to_dB(peak_right) << ", "
        "\"num_underruns\": " << m_num_underruns << ", "
        "\"num_overruns\": " << m_num_overruns << ", ";

    ss << "\"state\": ";

    switch (determineState()) {
        case input_state_t::NoData:
            ss << "\"NoData (1)\"";
            break;
        case input_state_t::Unstable:
            ss << "\"Unstable (2)\"";
            break;
        case input_state_t::Silence:
            ss << "\"Silent (3)\"";
            break;
        case input_state_t::Streaming:
            ss << "\"Streaming (4)\"";
            break;
    }

    ss << " } }";

    return ss.str();
}

input_state_t InputStat::determineState()
{
    const auto now = std::chrono::steady_clock::now();
    input_state_t state;

    /* if the last event was more that INPUT_COUNTER_RESET_TIME
     * ago, the timeout has expired. We can reset our
     * glitch counter.
     */
    if (now - m_time_last_event > INPUT_COUNTER_RESET_TIME) {
        m_glitch_counter = 0;
    }

    // STATE CALCULATION

    /* If the buffer has been empty for more than
     * INPUT_NODATA_TIMEOUT, we go to the NoData state.
     *
     * Consider an empty deque to be NoData too.
     */
    if (std::all_of(
                m_buffer_fill_stats.begin(), m_buffer_fill_stats.end(),
                [](const fill_stat_t& fs) { return fs.bufsize == 0; }) ) {
        state = input_state_t::NoData;
    }
    /* Otherwise, the state depends on the glitch counter */
    else if (m_glitch_counter >= INPUT_UNSTABLE_THRESHOLD) {
        state = input_state_t::Unstable;
    }
    else {
        /* The input is streaming, check if the audio level is too low */

        if (m_silence_counter > INPUT_AUDIO_LEVEL_SILENCE_COUNT) {
            state = input_state_t::Silence;
        }
        else {
            state = input_state_t::Streaming;
        }
    }

    return state;
}

