/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2019
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

#pragma once
#include <iostream>
#include <iterator>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include "ThreadsafeQueue.h"
#include "dabOutput/dabOutput.h"
#include "edioutput/TagItems.h"
#include "edioutput/TagPacket.h"
#include "edioutput/Transport.h"

// This metadata gets transmitted in the zmq stream
struct metadata_t {
    uint32_t edi_time;
    int16_t utc_offset;
    uint16_t dlfc;
};

using frame_t = std::pair<std::vector<uint8_t>, metadata_t>;

class EDISender {
    public:
        EDISender() = default;
        EDISender(const EDISender& other) = delete;
        EDISender& operator=(const EDISender& other) = delete;
        ~EDISender();
        void start(const edi::configuration_t& conf,
                int delay_ms, bool drop_late_packets);
        void push_frame(const frame_t& frame);
        void print_configuration(void);

    private:
        void send_eti_frame(uint8_t* p, metadata_t metadata);
        void process(void);

        int tist_delay_ms;
        bool drop_late;
        std::atomic<bool> running;
        std::thread process_thread;
        edi::configuration_t edi_conf;
        std::chrono::steady_clock::time_point startTime;
        ThreadsafeQueue<frame_t> frames;

        std::shared_ptr<edi::Sender> edi_sender;

        // For statistics about wait time before we transmit packets,
        // in microseconds
        std::vector<double> wait_times;

};
