/*
   Copyright (C) 2009 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2014 Matthias P. Braendli
   http://mpb.li

   A TCP Socket server that serves statistics for monitoring purposes.

   This server is very easy to integrate with munin http://munin-monitoring.org/
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
#include <sstream>
#include "StatsServer.h"
#include "Log.h"

void StatsServer::registerInput(std::string id)
{
    boost::mutex::scoped_lock lock(m_mutex);

    if (m_inputStats.count(id) == 1) {
        etiLog.level(error) <<
            "Double registration in Stats Server with id '" <<
            id << "'";
        return;
    }

    InputStat is;
    m_inputStats[id] = is;
}

void StatsServer::notifyBuffer(std::string id, long bufsize)
{
    boost::mutex::scoped_lock lock(m_mutex);

    if (m_inputStats.count(id) == 0) {
        etiLog.level(error) <<
            "Stats Server id '" <<
            id << "' does was not registered";
        return;
    }

    InputStat& is = m_inputStats[id];
    if (bufsize > is.max_fill_buffer) {
        is.max_fill_buffer = bufsize;
    }

    if (bufsize < is.min_fill_buffer ||
            is.min_fill_buffer == MIN_FILL_BUFFER_UNDEF) {
        is.min_fill_buffer = bufsize;
    }
}

void StatsServer::notifyUnderrun(std::string id)
{
    boost::mutex::scoped_lock lock(m_mutex);

    if (m_inputStats.count(id) == 0) {
        etiLog.level(error) <<
            "Stats Server id '" <<
            id << "' does was not registered";
        return;
    }

    InputStat& is = m_inputStats[id];
    is.num_underruns++;
}

void StatsServer::notifyOverrun(std::string id)
{
    boost::mutex::scoped_lock lock(m_mutex);

    if (m_inputStats.count(id) == 0) {
        etiLog.level(error) <<
            "Stats Server id '" <<
            id << "' does was not registered";
        return;
    }

    InputStat& is = m_inputStats[id];
    is.num_overruns++;
}

std::string StatsServer::getConfigJSON()
{
    std::ostringstream ss;
    ss << "{ \"config\" : [\n";

    std::map<std::string,InputStat>::iterator iter;
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

std::string StatsServer::getValuesJSON()
{
    std::ostringstream ss;
    ss << "{ \"values\" : {\n";

    std::map<std::string,InputStat>::iterator iter;
    int i = 0;
    for(iter = m_inputStats.begin(); iter != m_inputStats.end();
            ++iter, i++)
    {
        const std::string& id = iter->first;
        InputStat& stats = iter->second;

        if (i > 0) {
            ss << " ,\n";
        }

        ss << " \"" << id << "\" : ";
        ss << stats.encodeJSON();
        stats.reset();
    }

    ss << "}\n}\n";

    return ss.str();
}
void StatsServer::serverThread()
{
    int accepted_sock;
    char buffer[256];
    char welcome_msg[256];
    struct sockaddr_in serv_addr, cli_addr;
    int n;

    int welcome_msg_len = snprintf(welcome_msg, 256,
            "{ \"service\": \""
            "%s %s Stats Server\" }\n",
            PACKAGE_NAME, PACKAGE_VERSION);


    m_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (m_sock < 0) {
        etiLog.level(error) << "Error opening Stats Server socket: " <<
            strerror(errno);
        return;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY; // TODO listen only on 127.0.0.1
    serv_addr.sin_port = htons(m_listenport);

    if (bind(m_sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        etiLog.level(error) << "Error binding Stats Server socket: " <<
            strerror(errno);
        goto end_serverthread;
    }

    if (listen(m_sock, 5) < 0) {
        etiLog.level(error) << "Error listening on Stats Server socket: " <<
            strerror(errno);
        goto end_serverthread;
    }

    while (m_running)
    {
        socklen_t cli_addr_len = sizeof(cli_addr);

        /* Accept actual connection from the client */
        accepted_sock = accept(m_sock, (struct sockaddr *)&cli_addr, &cli_addr_len);
        if (accepted_sock < 0) {
            etiLog.level(warn) << "Stats Server cound not accept connection: " <<
                strerror(errno);
            continue;
        }
        /* Send welcome message with version */
        n = write(accepted_sock, welcome_msg, welcome_msg_len);
        if (n < 0) {
            etiLog.level(warn) << "Error writing to Stats Server socket " <<
                strerror(errno);
            close(accepted_sock);
            continue;
        }

        /* receive command */
        memset(buffer, 0, 256);
        int n = read(accepted_sock, buffer, 255);
        if (n < 0) {
            etiLog.level(warn) << "Error reading from Stats Server socket " <<
                strerror(errno);
            close(accepted_sock);
            continue;
        }

        if (strcmp(buffer, "config\n") == 0)
        {
            std::string json = getConfigJSON();
            n = write(accepted_sock, json.c_str(), json.size());
        }
        else if (strcmp(buffer, "values\n") == 0)
        {
            std::string json = getValuesJSON();
            n = write(accepted_sock, json.c_str(), json.size());
        }
        else
        {
            int len = snprintf(buffer, 256, "Invalid command\n");

            n = write(accepted_sock, buffer, len);
            if (n < 0) {
                etiLog.level(warn) << "Error writing to Stats Server socket " <<
                    strerror(errno);
            }
            close(accepted_sock);
            continue;
        }

        close(accepted_sock);
    }

end_serverthread:
    close(m_sock);
}


std::string InputStat::encodeJSON()
{
    std::ostringstream ss;

    ss <<
    "{ \"inputstat\" : {"
        "\"min_fill\": " << min_fill_buffer << ", "
        "\"max_fill\": " << max_fill_buffer << ", "
        "\"num_underruns\": " << num_underruns << ", "
        "\"num_overruns\": " << num_overruns <<
    " } }";

    return ss.str();
}

