/*
   Copyright (C) 2009 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2014, 2015
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
#include <ctime>
#include <boost/thread.hpp>
#include "ManagementServer.h"
#include "Log.h"

void ManagementServer::registerInput(InputStat* is)
{
    boost::mutex::scoped_lock lock(m_statsmutex);

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
    boost::mutex::scoped_lock lock(m_statsmutex);

    if (m_inputStats.count(id) == 1) {
        m_inputStats.erase(id);
    }
}


bool ManagementServer::isInputRegistered(std::string& id)
{
    boost::mutex::scoped_lock lock(m_statsmutex);

    if (m_inputStats.count(id) == 0) {
        etiLog.level(error) <<
            "Stats Server id '" <<
            id << "' does was not registered";
        return false;
    }
    return true;
}

std::string ManagementServer::getStatConfigJSON()
{
    boost::mutex::scoped_lock lock(m_statsmutex);

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
    boost::mutex::scoped_lock lock(m_statsmutex);

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
        stats->reset();
    }

    ss << "}\n}\n";

    return ss.str();
}

std::string ManagementServer::getStateJSON()
{
    boost::mutex::scoped_lock lock(m_statsmutex);

    std::ostringstream ss;
    ss << "{\n";

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
        ss << stats->encodeStateJSON();
        stats->reset();
    }

    ss << "}\n";

    return ss.str();
}

void ManagementServer::restart()
{
    m_restarter_thread = boost::thread(&ManagementServer::restart_thread,
            this, 0);
}

// This runs in a separate thread, because
// it would take too long to be done in the main loop
// thread.
void ManagementServer::restart_thread(long)
{
    m_running = false;

    if (m_listenport) {
        m_thread.interrupt();
        m_thread.join();
    }

    m_thread = boost::thread(&ManagementServer::serverThread, this);
}

void ManagementServer::serverThread()
{
    m_fault = false;

    try {
        int accepted_sock;
        char buffer[256];
        char welcome_msg[256];
        struct sockaddr_in serv_addr, cli_addr;
        int n;

        int welcome_msg_len = snprintf(welcome_msg, 256,
                "{ \"service\": \""
                "%s %s MGMT Server\" }\n",
                PACKAGE_NAME,
#if defined(GITVERSION)
                GITVERSION
#else
                PACKAGE_VERSION
#endif
                );


        m_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (m_sock < 0) {
            etiLog.level(error) <<
                "Error opening MGMT Server socket: " <<
                strerror(errno);
            m_fault = true;
            return;
        }

        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = INADDR_ANY; // TODO listen only on 127.0.0.1
        serv_addr.sin_port = htons(m_listenport);

        if (bind(m_sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
            etiLog.level(error) <<
                "Error binding MGMT Server socket: " <<
                strerror(errno);
            goto end_serverthread;
        }

        if (listen(m_sock, 5) < 0) {
            etiLog.level(error) <<
                "Error listening on MGMT Server socket: " <<
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
                etiLog.level(warn) << "MGMT Server cound not accept connection: " <<
                    strerror(errno);
                continue;
            }
            /* Send welcome message with version */
            n = write(accepted_sock, welcome_msg, welcome_msg_len);
            if (n < 0) {
                etiLog.level(warn) <<
                    "MGMT: Error writing to Stats Server socket " <<
                    strerror(errno);
                close(accepted_sock);
                continue;
            }

            /* receive command */
            memset(buffer, 0, 256);
            int n = read(accepted_sock, buffer, 255);
            if (n < 0) {
                etiLog.level(warn) <<
                    "MGMT: Error reading from Stats Server socket " <<
                    strerror(errno);
                close(accepted_sock);
                continue;
            }

            if (strcmp(buffer, "config\n") == 0) {
                std::string json = getStatConfigJSON();
                n = write(accepted_sock, json.c_str(), json.size());
            }
            else if (strcmp(buffer, "values\n") == 0) {
                std::string json = getValuesJSON();
                n = write(accepted_sock, json.c_str(), json.size());
            }
            else if (strcmp(buffer, "state\n") == 0) {
                std::string json = getStateJSON();
                n = write(accepted_sock, json.c_str(), json.size());
            }
            else if (strcmp(buffer, "setptree\n") == 0) {
                const ssize_t max_json_len = 32768;
                char json[max_json_len] = {'\0'};

                ssize_t json_len = read(accepted_sock, json, max_json_len);

                if (json_len < max_json_len) {
                    boost::unique_lock<boost::mutex> lock(m_configmutex);

                    std::stringstream ss;
                    ss << json;

                    m_pt.clear();
                    boost::property_tree::json_parser::read_json(ss, m_pt);
                }
                else if (json_len == 0) {
                    etiLog.level(warn) <<
                        "MGMT: No JSON data received";
                }
                else if (json_len < 0) {
                    etiLog.level(warn) <<
                        "MGMT: JSON data receive error: " <<
                        strerror(errno);
                }
                else {
                    etiLog.level(warn) <<
                        "MGMT: Received JSON too large";
                }

            }
            else if (strcmp(buffer, "getptree\n") == 0) {
                boost::unique_lock<boost::mutex> lock(m_configmutex);
                m_pending = true;

                while (m_pending && !m_retrieve_pending) {
                    m_condition.wait(lock);
                }
                std::stringstream ss;
                boost::property_tree::json_parser::write_json(ss, m_pt);

                std::string response = ss.str();

                n = write(accepted_sock, response.c_str(), response.size());
            }
            else {
                int len = snprintf(buffer, 256, "Invalid command\n");
                n = write(accepted_sock, buffer, len);
            }

            if (n < 0) {
                etiLog.level(warn) <<
                    "Error writing to MGMT Server socket " <<
                    strerror(errno);
            }
            close(accepted_sock);
        }

end_serverthread:
        m_fault = true;
        close(m_sock);

    }
    catch (std::exception& e) {
        etiLog.level(error) <<
            "MGMT server caught exception: " <<
            e.what();
        m_fault = true;
    }
}

bool ManagementServer::retrieve_new_ptree(boost::property_tree::ptree& pt)
{
    boost::unique_lock<boost::mutex> lock(m_configmutex);

    if (m_retrieve_pending)
    {
        pt = m_pt;

        m_retrieve_pending = false;
        m_condition.notify_one();
        return true;
    }

    return false;
}

void ManagementServer::update_ptree(const boost::property_tree::ptree& pt)
{
    if (m_running) {
        boost::unique_lock<boost::mutex> lock(m_configmutex);
        m_pt = pt;
        m_pending = false;

        m_condition.notify_one();
    }
}

/************************************************/

void InputStat::registerAtServer()
{
    mgmt_server->registerInput(this);
}

InputStat::~InputStat()
{
    mgmt_server->unregisterInput(m_name);
}

std::string InputStat::encodeValuesJSON()
{
    std::ostringstream ss;

    const int16_t int16_max = std::numeric_limits<int16_t>::max();

    boost::mutex::scoped_lock lock(m_mutex);

    /* convert to dB */
    int dB_l = peak_left  ? round(20*log10((double)peak_left / int16_max))  : -90;
    int dB_r = peak_right ? round(20*log10((double)peak_right / int16_max)) : -90;

    ss <<
    "{ \"inputstat\" : {"
        "\"min_fill\": " << min_fill_buffer << ", "
        "\"max_fill\": " << max_fill_buffer << ", "
        "\"peak_left\": " << dB_l << ", "
        "\"peak_right\": " << dB_r << ", "
        "\"num_underruns\": " << num_underruns << ", "
        "\"num_overruns\": " << num_overruns <<
    " } }";

    return ss.str();
}

std::string InputStat::encodeStateJSON()
{
    std::ostringstream ss;

    ss << "{ \"state\" : ";

    switch (determineState()) {
        case NoData:
            ss << "\"NoData\"";
            break;
        case Unstable:
            ss << "\"Unstable\"";
            break;
        case Silence:
            ss << "\"Silent\"";
            break;
        case Streaming:
            ss << "\"Streaming\"";
            break;
        default:
            ss << "\"Unknown\"";
    }

    ss << " }";

    return ss.str();
}

input_state_t InputStat::determineState(void)
{
    boost::mutex::scoped_lock lock(m_mutex);

    time_t now = time(NULL);
    input_state_t state;

    /* if the last event was more that INPUT_COUNTER_RESET_TIME
     * minutes ago, the timeout has expired. We can reset our
     * glitch counter.
     */
    if (now - m_time_last_event > 60*INPUT_COUNTER_RESET_TIME) {
        m_glitch_counter = 0;
    }

    // STATE CALCULATION

    /* If the buffer has been empty for more than
     * INPUT_NODATA_TIMEOUT, we go to the NoData state.
     */
    if (m_buffer_empty &&
            now - m_time_last_buffer_nonempty > INPUT_NODATA_TIMEOUT) {
        state = NoData;
    }

    /* Otherwise, the state depends on the glitch counter */
    else if (m_glitch_counter >= INPUT_UNSTABLE_THRESHOLD) {
        state = Unstable;
    }
    else {
        /* The input is streaming, check if the audio level is too low */

        if (m_silence_counter > INPUT_AUDIO_LEVEL_SILENCE_COUNT) {
            state = Silence;
        }
        else {
            state = Streaming;
        }
    }

    return state;
}

