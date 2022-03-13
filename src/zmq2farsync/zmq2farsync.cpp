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

#include "dabOutput/dabOutput.h"
#include "Log.h"
#include "zmq.hpp"
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

constexpr size_t MAX_ERROR_COUNT = 10;
constexpr size_t MAX_NUM_RESETS = 180;
constexpr long ZMQ_TIMEOUT_MS = 1000;

static void usage()
{
    using namespace std;

    cerr << "Usage:" << endl;
    cerr << "odr-zmq2farsync <source> <destination>" << endl << endl;

    cerr << "Where" << endl;
    cerr << " <source> is a ZMQ URL that points to a ODR-DabMux ZMQ output." << endl;
    cerr << " <destination> is the device information for the FarSync card." << endl << endl;
    cerr << " The syntax is the same as for ODR-DabMux" << endl << endl;

    cerr << "The input socket will be reset if no data is received for " <<
        (int)(MAX_ERROR_COUNT * ZMQ_TIMEOUT_MS / 1000.0) << " seconds." << endl;
    cerr << "After " << MAX_NUM_RESETS << " consecutive resets, the process will quit." << endl;
    cerr << "It is best practice to run this tool under a process supervisor that will restart it automatically." << endl;
}

int main(int argc, char **argv)
{
    // Version handling is done very early to ensure nothing else but the version gets printed out
    if (argc == 2 and strcmp(argv[1], "--version") == 0) {
        fprintf(stdout, "%s\n",
#if defined(GITVERSION)
                GITVERSION
#else
                PACKAGE_VERSION
#endif
               );
        return 0;
    }

    etiLog.level(info) << "ZMQ2FarSync ETI converter from " <<
        PACKAGE_NAME << " " <<
#if defined(GITVERSION)
        GITVERSION <<
#else
        PACKAGE_VERSION <<
#endif
        " starting up";

    if (argc != 3) {
        usage();
        return 1;
    }

    const char* source_url = argv[1];
    const char* destination_device = argv[2];

    etiLog.level(info) << "Opening FarSync card: " << destination_device;
    DabOutputRaw output;
    if (output.Open(destination_device) == -1) {
        etiLog.level(error) <<
            "Unable to open output ";
        return -1;
    }

    etiLog.level(info) << "Opening ZMQ input: " << source_url;
    zmq::context_t zmq_ctx(1);
    size_t frame_count = 0;
    size_t loop_counter = 0;
    size_t num_consecutive_resets = 0;
    while (num_consecutive_resets < MAX_NUM_RESETS) {
        zmq::socket_t zmq_sock(zmq_ctx, ZMQ_SUB);
        zmq_sock.connect(source_url);
        zmq_sock.setsockopt(ZMQ_SUBSCRIBE, NULL, 0); // subscribe to all messages

        size_t error_count = 0;
        while (error_count < MAX_ERROR_COUNT) {
            zmq::message_t incoming;
            zmq::pollitem_t items[1];
            items[0].socket = zmq_sock;
            items[0].events = ZMQ_POLLIN;
            const int num_events = zmq::poll(items, 1, ZMQ_TIMEOUT_MS);
            if (num_events == 0) { // timeout
                error_count++;
            }
            else {
                num_consecutive_resets = 0;

                // Event received: recv will not block
                zmq_sock.recv(&incoming);
                zmq_dab_message_t* dab_msg = (zmq_dab_message_t*)incoming.data();

                if (dab_msg->version != 1) {
                    etiLog.level(error) << "ZeroMQ wrong packet version " << dab_msg->version;
                    error_count++;
                }

                int offset = sizeof(dab_msg->version) +
                    NUM_FRAMES_PER_ZMQ_MESSAGE * sizeof(*dab_msg->buflen);

                for (int i = 0; i < NUM_FRAMES_PER_ZMQ_MESSAGE; i++) {
                    if (dab_msg->buflen[i] <= 0 or dab_msg->buflen[i] > 6144) {
                        etiLog.level(error) << "ZeroMQ buffer " << i << " has invalid length " <<
                            dab_msg->buflen[i];
                        error_count++;
                    }
                    else {
                        std::vector<uint8_t> buf(6144, 0x55);

                        const int framesize = dab_msg->buflen[i];

                        memcpy(&buf.front(),
                                ((uint8_t*)incoming.data()) + offset,
                                framesize);

                        offset += framesize;

                        if (output.Write(&buf.front(), buf.size()) == -1) {
                            etiLog.level(error) << "Cannot write to output!";
                            error_count++;
                        }

                        frame_count++;
                    }
                }

                loop_counter++;
                if (loop_counter > 250) {
                    etiLog.level(info) << "Transmitted " << frame_count << " ETI frames";
                    loop_counter = 0;
                }
            }
        }

        num_consecutive_resets++;

        zmq_sock.close();
        std::this_thread::sleep_for(std::chrono::seconds(1));
        etiLog.level(info) << "ZMQ input (" << source_url << ") timeout after " <<
            num_consecutive_resets << " consecutive resets.";
    }

    return 0;
}

