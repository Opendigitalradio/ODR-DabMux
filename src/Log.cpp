/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)

   Copyright (C) 2018
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://www.opendigitalradio.org
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

#include <list>
#include <cstdarg>
#include <chrono>

#include "Log.h"

using namespace std;

/* etiLog is a singleton used in all parts of ODR-DabMod to output log messages.
 */
Logger etiLog;

void Logger::register_backend(std::shared_ptr<LogBackend> backend)
{
    backends.push_back(backend);
}


void Logger::log(log_level_t level, const char* fmt, ...)
{
    if (level == discard) {
        return;
    }

    int size = 100;
    std::string str;
    va_list ap;
    while (1) {
        str.resize(size);
        va_start(ap, fmt);
        int n = vsnprintf((char *)str.c_str(), size, fmt, ap);
        va_end(ap);
        if (n > -1 && n < size) {
            str.resize(n);
            break;
        }
        if (n > -1)
            size = n + 1;
        else
            size *= 2;
    }

    logstr(level, move(str));
}

void Logger::logstr(log_level_t level, std::string&& message)
{
    if (level == discard) {
        return;
    }

    /* Remove a potential trailing newline.
     * It doesn't look good in syslog
     */
    if (message[message.length()-1] == '\n') {
        message.resize(message.length()-1);
    }

    for (auto &backend : backends) {
        backend->log(level, message);
    }

    {
        std::lock_guard<std::mutex> guard(m_cerr_mutex);
        std::cerr << levels_as_str[level] << " " << message << std::endl;
    }
}


LogLine Logger::level(log_level_t level)
{
    return LogLine(this, level);
}

LogToFile::LogToFile(const std::string& filename) : name("FILE")
{
    FILE* fd = fopen(filename.c_str(), "a");
    if (fd == nullptr) {
        fprintf(stderr, "Cannot open log file !");
        throw std::runtime_error("Cannot open log file !");
    }

    log_file.reset(fd);
}

void LogToFile::log(log_level_t level, const std::string& message)
{
    if (level != log_level_t::discard) {
        const char* log_level_text[] = {
            "DEBUG", "INFO", "WARN", "ERROR", "ALERT", "EMERG"};

        // fprintf is thread-safe
        fprintf(log_file.get(), SYSLOG_IDENT ": %s: %s\n",
                log_level_text[(size_t)level], message.c_str());
        fflush(log_file.get());
    }
}

void LogToSyslog::log(log_level_t level, const std::string& message)
{
    if (level != log_level_t::discard) {
        int syslog_level = LOG_EMERG;
        switch (level) {
            case debug: syslog_level = LOG_DEBUG; break;
            case info:  syslog_level = LOG_INFO; break;
                        /* we don't have the notice level */
            case warn:  syslog_level = LOG_WARNING; break;
            case error: syslog_level = LOG_ERR; break;
            default:    syslog_level = LOG_CRIT; break;
            case alert: syslog_level = LOG_ALERT; break;
            case emerg: syslog_level = LOG_EMERG; break;
        }

        syslog(syslog_level, SYSLOG_IDENT " %s", message.c_str());
    }
}
