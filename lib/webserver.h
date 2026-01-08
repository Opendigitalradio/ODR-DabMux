/*
   Copyright (C) 2025
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://www.opendigitalradio.org
 */
/*
   This file is part of the ODR-mmbTools.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once
#include <cmath>
#include <atomic>
#include <cstring>
#include <cstdint>

#include "Socket.h"

class WebServer {
    public:
        WebServer(std::string listen_ip, uint16_t port, const std::string& index_content);
        virtual ~WebServer();
        WebServer(const WebServer&) = delete;
        WebServer& operator=(const WebServer&) = delete;

        void update_stats_json(const std::string& new_stats_json);

    private:
        void serve();
        bool dispatch_client(Socket::TCPSocket&& sock);
        bool send_index(Socket::TCPSocket& s);
        bool send_stats(Socket::TCPSocket& s);

        Socket::TCPSocket server_socket;

        std::thread handler_thread;
        std::atomic<bool> running = ATOMIC_VAR_INIT(true);

        std::string index_content;

        mutable std::mutex data_mutex;
        std::string stats_json;
};
