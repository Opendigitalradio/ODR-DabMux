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

#include "webserver.h"
#include <future>

#include "Log.h"

using namespace std;

static const char* http_ok = "HTTP/1.0 200 OK\r\n";
static const char* http_404 = "HTTP/1.0 404 Not Found\r\n";
/* unused:
static const char* http_400 = "HTTP/1.0 400 Bad Request\r\n";
static const char* http_405 = "HTTP/1.0 405 Method Not Allowed\r\n";
static const char* http_500 = "HTTP/1.0 500 Internal Server Error\r\n";
static const char* http_503 = "HTTP/1.0 503 Service Unavailable\r\n";
static const char* http_contenttype_data = "Content-Type: application/octet-stream\r\n";
static const char* http_contenttype_html = "Content-Type: text/html; charset=utf-8\r\n";
*/
static const char* http_contenttype_text = "Content-Type: text/plain\r\n";
static const char* http_contenttype_json = "Content-Type: application/json; charset=utf-8\r\n";

static const char* http_nocache = "Cache-Control: no-cache\r\n";

WebServer::WebServer(std::string listen_ip, uint16_t port, const std::string& index_content)
    : index_content(index_content)
{
    server_socket.listen(port, listen_ip);

    handler_thread = thread(&WebServer::serve, this);
}

WebServer::~WebServer()
{
    running = false;

    if (handler_thread.joinable()) {
        handler_thread.join();
    }

    server_socket.close();
}

void WebServer::update_stats_json(const std::string& new_stats_json)
{
    unique_lock<mutex> lock(data_mutex);
    stats_json = new_stats_json;
}

void WebServer::serve()
{
    deque<future<bool> > running_connections;

    while (running) {
        auto sock = server_socket.accept(1000);

        if (sock.valid()) {
            running_connections.push_back(async(launch::async,
                        &WebServer::dispatch_client, this, std::move(sock)));

            deque<future<bool> > still_running_connections;
            for (auto& fut : running_connections) {
                if (fut.valid()) {
                    switch (fut.wait_for(chrono::milliseconds(1))) {
                        case future_status::deferred:
                        case future_status::timeout:
                            still_running_connections.push_back(std::move(fut));
                            break;
                        case future_status::ready:
                            fut.get();
                            break;
                    }
                }
            }
            running_connections = std::move(still_running_connections);
        }
    }

    etiLog.level(info) << "WebServer draining connections";
    while (running_connections.size() > 0) {
        deque<future<bool> > still_running_connections;
        for (auto& fut : running_connections) {
            if (fut.valid()) {
                switch (fut.wait_for(chrono::milliseconds(1))) {
                    case future_status::deferred:
                    case future_status::timeout:
                        still_running_connections.push_back(std::move(fut));
                        break;
                    case future_status::ready:
                        fut.get();
                        break;
                }
            }
        }
        running_connections = std::move(still_running_connections);
    }
}

static string recv_line(Socket::TCPSocket& s) {
    string line;
    bool cr_seen = false;

    while (true) {
        char c = 0;
        ssize_t ret = s.recv(&c, 1, 0);
        if (ret == 0) {
            return "";
        }
        else if (ret == -1) {
            string errstr = strerror(errno);
            etiLog.level(error) << "recv error " << errstr;
            return "";
        }

        line += c;

        if (c == '\r') {
            cr_seen = true;
        }
        else if (cr_seen and c == '\n') {
            return line;
        }
    }
}

static vector<char> recv_exactly(Socket::TCPSocket& s, size_t num_bytes)
{
    vector<char> buf(num_bytes);
    size_t rx = 0;

    while (rx < num_bytes) {
        const size_t remain = num_bytes - rx;
        ssize_t ret = s.recv(buf.data() + rx, remain, 0);

        if (ret == 0) {
            break;
        }
        else if (ret == -1) {
            string errstr = strerror(errno);
            etiLog.level(error) << "recv error " << errstr;
            return {};
        }
        else {
            rx += ret;
        }
    }

    return buf;
}

static vector<string> split(const string& str, char c = ' ')
{
    const char *s = str.data();
    vector<string> result;
    do {
        const char *begin = s;
        while (*s != c && *s)
            s++;
        result.push_back(string(begin, s));
    } while (0 != *s++);
    return result;
}

struct http_request_t {
    bool valid = false;

    bool is_get = false;
    bool is_post = false;
    string url;
    map<string, string> headers;
    string post_data;
};

static http_request_t parse_http_headers(Socket::TCPSocket& s) {
    http_request_t r;

    const auto first_line = recv_line(s);
    const auto request_type = split(first_line);

    if (request_type.size() != 3) {
        return r;
    }
    else if (request_type[0] == "GET") {
        r.is_get = true;
    }
    else if (request_type[0] == "POST") {
        r.is_post = true;
    }
    else {
        return r;
    }

    r.url = request_type[1];

    while (true) {
        string header_line = recv_line(s);

        if (header_line == "\r\n") {
            break;
        }

        const auto header = split(header_line, ':');

        if (header.size() == 2) {
            r.headers.emplace(header[0], header[1]);
        }
    }

    if (r.is_post) {
        constexpr auto CONTENT_LENGTH = "Content-Length";
        if (r.headers.count(CONTENT_LENGTH) == 1) {
            try {
                const int content_length = stoi(r.headers[CONTENT_LENGTH]);
                if (content_length > 1024 * 1024) {
                    etiLog.level(warn) << "Unreasonable POST Content-Length: " << content_length;
                    return r;
                }

                const auto buf = recv_exactly(s, content_length);
                r.post_data = string(buf.begin(), buf.end());
            }
            catch (const invalid_argument&) {
                etiLog.level(warn) << "Cannot parse POST Content-Length: " << r.headers[CONTENT_LENGTH];
                return r;
            }
            catch (const out_of_range&) {
                etiLog.level(warn) << "Cannot represent POST Content-Length: " << r.headers[CONTENT_LENGTH];
                return r;
            }
        }
    }

    r.valid = true;
    return r;
}

static bool send_http_response(
        Socket::TCPSocket& s,
        const string& statuscode,
        const string& data,
        const string& content_type = http_contenttype_text)
{
    string headers = statuscode;
    headers += content_type;
    headers += http_nocache;
    headers += "\r\n";
    headers += data;
    ssize_t ret = s.send(headers.data(), headers.size(), MSG_NOSIGNAL);
    if (ret == -1) {
        etiLog.level(warn) << "Failed to send response " << statuscode << " " << data;
    }
    return ret != -1;
}

bool WebServer::dispatch_client(Socket::TCPSocket&& sock)
{
    try {
        Socket::TCPSocket s(std::move(sock));

        bool success = false;

        if (not s.valid()) {
            etiLog.level(error) << "socket in dispatcher not valid!";
            return false;
        }

        const auto req = parse_http_headers(s);

        if (not req.valid) {
            return false;
        }

        if (req.is_get) {
            if (req.url == "/") {
                success = send_index(s);
            }
            else if (req.url == "/stats.json") {
                success = send_stats(s);
            }
        }
        else if (req.is_post) {
            if (req.url == "/rc") {
                //success = handle_rc(s, req.post_data);
            }
            else {
                etiLog.level(warn) << "Could not understand POST request " << req.url;
            }
        }
        else {
            throw logic_error("valid req is neither GET nor POST!");
        }

        if (not success) {
            send_http_response(s, http_404, "Could not understand request.\r\n");
        }

        return success;
    }
    catch (const std::exception& e)
    {
        return false;
    }
}

bool WebServer::send_index(Socket::TCPSocket& s)
{
    if (not send_http_response(s, http_ok, "", http_contenttype_text)) {
        return false;
    }

    ssize_t ret = s.send(index_content.c_str(), index_content.size(), MSG_NOSIGNAL);
    if (ret == -1) {
        etiLog.level(warn) << "Failed to send index";
        return false;
    }
    return true;
}

bool WebServer::send_stats(Socket::TCPSocket& s)
{
    if (not send_http_response(s, http_ok, "", http_contenttype_json)) {
        return false;
    }

    std::string jsonstr = "{ }";
    {
        unique_lock<mutex> lock(data_mutex);
        if (not stats_json.empty()) {
            jsonstr = stats_json;
        }
    }

    ssize_t ret = s.send(jsonstr.c_str(), jsonstr.size(), MSG_NOSIGNAL);
    if (ret == -1) {
        etiLog.level(warn) << "Failed to send index";
        return false;
    }
    return true;
}
