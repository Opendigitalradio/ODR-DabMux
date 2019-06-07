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

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include <stdlib.h>
#include <memory>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/info_parser.hpp>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <cstring>
#include <string>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

// for basename
#include <libgen.h>

#include <iterator>
#include <vector>
#include <list>
#include <set>
#include <map>
#include <functional>
#include <algorithm>

#ifdef _WIN32
#   include <time.h>
#   include <process.h>
#   include <io.h>
#   include <conio.h>
#   include <winsock2.h> // For types...
typedef u_char uint8_t;
typedef WORD uint16_t;
typedef DWORD32 uint32_t;

#   ifndef __MINGW32__
#       include "xgetopt.h"
#   endif
#   define read _read
#   define snprintf _snprintf 
#   define sleep(a) Sleep((a) * 1000)
#else
#   include <netinet/in.h>
#   include <unistd.h>
#   include <sys/time.h>
#   include <sys/wait.h>
#   include <sys/socket.h>
#   include <sys/ioctl.h>
#   include <sys/times.h>
#   include <sys/resource.h>

#endif

#include <time.h>

#ifdef _WIN32
#   pragma warning ( disable : 4103 )
#   include "Eti.h"
#   pragma warning ( default : 4103 )
#else
#   include "Eti.h"
#endif

#include "input/Prbs.h"
#include "input/Zmq.h"

#include "dabOutput/dabOutput.h"
#include "crc.h"
#include "Socket.h"
#include "PcDebug.h"
#include "DabMux.h"
#include "MuxElements.h"
#include "utils.h"
#include "ConfigParser.h"
#include "ManagementServer.h"
#include "Log.h"
#include "RemoteControl.h"

using namespace std;
using boost::property_tree::ptree;
using boost::property_tree::ptree_error;

volatile sig_atomic_t running = 1;

/* We are not allowed to use etiLog in the signal handler,
 * because etiLog uses mutexes
 */
void signalHandler(int signum)
{
#ifdef _WIN32
    fprintf(stderr, "\npid: %i\n", _getpid());
#else
    fprintf(stderr, "\npid: %i, ppid: %i\n", getpid(), getppid());
#endif
#define SIG_MSG "Signal received: "
    switch (signum) {
#ifndef _WIN32
    case SIGHUP:
        fprintf(stderr, SIG_MSG "SIGHUP\n");
        break;
    case SIGQUIT:
        fprintf(stderr, SIG_MSG "SIGQUIT\n");
        break;
    case SIGPIPE:
        fprintf(stderr, SIG_MSG "SIGPIPE\n");
        return;
        break;
#endif
    case SIGINT:
        fprintf(stderr, SIG_MSG "SIGINT\n");
        break;
    case SIGTERM:
        fprintf(stderr, SIG_MSG "SIGTERM\n");
        fprintf(stderr, "Exiting software\n");
        exit(0);
        break;
    default:
        fprintf(stderr, SIG_MSG "number %i\n", signum);
    }
#ifndef _WIN32
    killpg(0, SIGPIPE);
#endif
    running = 0;
}


int main(int argc, char *argv[])
{
    header_message();

    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = &signalHandler;

    const int sigs[] = {SIGHUP, SIGQUIT, SIGINT, SIGTERM};
    for (int sig : sigs) {
        if (sigaction(sig, &sa, nullptr) == -1) {
            perror("sigaction");
            return EXIT_FAILURE;
        }
    }

#ifdef _WIN32
    if (SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST) == 0) {
        etiLog.log(warn, "Can't increase priority: %s\n",
                strerror(errno));
    }
#else
    // Use the lowest real-time priority for this thread, and switch to real-time scheduling
    const int policy = SCHED_RR;
    sched_param sp;
    sp.sched_priority = sched_get_priority_min(policy);
    int thread_prio_ret = pthread_setschedparam(pthread_self(), policy, &sp);
    if (thread_prio_ret != 0) {
        etiLog.level(error) << "Could not set real-time priority for thread:" << thread_prio_ret;
    }
#endif




    int returnCode = 0;

    ptree pt;
    std::vector<std::shared_ptr<DabOutput> > outputs;

    try {
        if (argc == 2) { // Assume the only argument is a config file
            string conf_file = argv[1];

            if (conf_file == "-h") {
                printUsage(argv[0], stdout);
                throw MuxInitException("Nothing to do");
            }

            try {
                read_info(conf_file, pt);

            }
            catch (runtime_error &e) {
                throw MuxInitException(e.what());
            }
        }
        else if (argc > 1 && strncmp(argv[1], "-e", 2) == 0) { // use external config file
            try {

                if (argc != 3) {
                    printUsage(argv[0], stderr);
                    throw MuxInitException();
                }

                string conf_file = argv[2];

                read_info(conf_file, pt);
            }
            catch (runtime_error &e) {
                throw MuxInitException(e.what());
            }
        }
        else {
            throw MuxInitException("No configuration file specified");
        }

        /* Enable Logging to syslog conditionally */
        if (pt.get<bool>("general.syslog", false)) {
            etiLog.register_backend(std::make_shared<LogToSyslog>());
        }

        int mgmtserverport = pt.get<int>("general.managementport",
                             pt.get<int>("general.statsserverport", 0) );

        /* Management: stats and config server */
        get_mgmt_server().open(mgmtserverport);

        /************** READ REMOTE CONTROL PARAMETERS *************/
        int telnetport = pt.get<int>("remotecontrol.telnetport", 0);
        if (telnetport != 0) {
            auto rc = std::make_shared<RemoteControllerTelnet>(telnetport);
            rcs.add_controller(rc);
        }

        auto zmqendpoint = pt.get<string>("remotecontrol.zmqendpoint", "");
        if (not zmqendpoint.empty()) {
            auto rc = std::make_shared<RemoteControllerZmq>(zmqendpoint);
            rcs.add_controller(rc);
        }

        DabMultiplexer mux(pt);

        etiLog.level(info) <<
                PACKAGE_NAME << " " <<
#if defined(GITVERSION)
                GITVERSION <<
#else
                PACKAGE_VERSION <<
#endif
                " starting up";


        edi::configuration_t edi_conf;

        /******************** READ OUTPUT PARAMETERS ***************/
        set<string> all_output_names;
        bool output_require_tai_clock = false;
        ptree pt_outputs = pt.get_child("outputs");
        for (auto ptree_pair : pt_outputs) {
            string outputuid = ptree_pair.first;

            // check for uniqueness of the uid
            if (all_output_names.count(outputuid) == 0) {
                all_output_names.insert(outputuid);
            }
            else {
                stringstream ss;
                ss << "output with uid " << outputuid << " not unique!";
                throw runtime_error(ss.str());
            }

            if (outputuid == "edi") {
                ptree pt_edi = pt_outputs.get_child("edi");
                for (auto pt_edi_dest : pt_edi.get_child("destinations")) {
                    const auto proto = pt_edi_dest.second.get<string>("protocol");
                    if (proto == "udp") {
                        auto dest = make_shared<edi::udp_destination_t>();
                        dest->dest_addr   = pt_edi_dest.second.get<string>("destination");
                        dest->ttl         = pt_edi_dest.second.get<unsigned int>("ttl", 1);

                        dest->source_addr = pt_edi_dest.second.get<string>("source", "");
                        dest->source_port = pt_edi_dest.second.get<unsigned int>("sourceport");

                        edi_conf.destinations.push_back(dest);
                    }
                    else if (proto == "tcp") {
                        auto dest = make_shared<edi::tcp_server_t>();
                        dest->listen_port = pt_edi_dest.second.get<unsigned int>("listenport");
                        dest->max_frames_queued = pt_edi_dest.second.get<size_t>("max_frames_queued", 500);
                        edi_conf.destinations.push_back(dest);
                    }
                    else {
                        throw runtime_error("Unknown EDI protocol " + proto);
                    }
                }

                edi_conf.dest_port           = pt_edi.get<unsigned int>("port");

                edi_conf.dump                = pt_edi.get<bool>("dump");
                edi_conf.enable_pft          = pt_edi.get<bool>("enable_pft");
                edi_conf.verbose             = pt_edi.get<bool>("verbose");

                edi_conf.fec                 = pt_edi.get<unsigned int>("fec", 3);
                edi_conf.chunk_len           = pt_edi.get<unsigned int>("chunk_len", 207);

                double interleave_ms         = pt_edi.get<double>("interleave", 0);
                if (interleave_ms != 0.0) {
                    if (interleave_ms < 0) {
                        throw runtime_error("EDI output: negative interleave value is invalid.");
                    }

                    auto latency_rounded = lround(interleave_ms / 24.0);
                    if (latency_rounded * 24 > 30000) {
                        throw runtime_error("EDI output: interleaving set for more than 30 seconds!");
                    }

                    edi_conf.latency_frames = latency_rounded;
                }

                edi_conf.tagpacket_alignment = pt_edi.get<unsigned int>("tagpacket_alignment", 8);

                mux.set_edi_config(edi_conf);
            }
            else if (outputuid == "zeromq") {
#if defined(HAVE_OUTPUT_ZEROMQ)
                ptree pt_zeromq = pt_outputs.get_child("zeromq");
                shared_ptr<DabOutput> output;

                string endpoint = pt_zeromq.get<string>("endpoint");
                bool allow_metadata = pt_zeromq.get<bool>("allowmetadata");
                output_require_tai_clock |= allow_metadata;

                size_t proto_pos = endpoint.find("://");
                if (proto_pos == std::string::npos) {
                    stringstream ss;
                    ss << "zeromq output endpoint '" << endpoint << "' has incorrect format!";
                    throw runtime_error(ss.str());
                }

                string proto = endpoint.substr(0, proto_pos);
                string location = endpoint.substr(proto_pos + 3);

                output = make_shared<DabOutputZMQ>(proto, allow_metadata);

                if (not output) {
                    etiLog.level(error) <<
                        "Unable to init zeromq output " <<
                        endpoint;
                    return -1;
                }

                if (output->Open(location) == -1) {
                    etiLog.level(error) <<
                        "Unable to open zeromq output " <<
                        endpoint;
                    return -1;
                }

                outputs.push_back(output);
#else
                throw runtime_error("ZeroMQ output not compiled in");
#endif
            }
            else {
                string uri = pt_outputs.get<string>(outputuid);

                size_t proto_pos = uri.find("://");
                if (proto_pos == std::string::npos) {
                    stringstream ss;
                    ss << "Output with uid " << outputuid << " no protocol defined!";
                    throw runtime_error(ss.str());
                }

                string proto = uri.substr(0, proto_pos);
                string location = uri.substr(proto_pos + 3);

                std::shared_ptr<DabOutput> output;

                if (0) {
#if defined(HAVE_OUTPUT_FILE)
                } else if (proto == "file") {
                    output = make_shared<DabOutputFile>();
#endif // defined(HAVE_OUTPUT_FILE)
#if defined(HAVE_OUTPUT_FIFO)
                } else if (proto == "fifo") {
                    output = make_shared<DabOutputFifo>();
#endif // !defined(HAVE_OUTPUT_FIFO)
#if defined(HAVE_OUTPUT_RAW)
                } else if (proto == "raw") {
                    output = make_shared<DabOutputRaw>();
#endif // defined(HAVE_OUTPUT_RAW)
#if defined(HAVE_OUTPUT_UDP)
                } else if (proto == "udp") {
                    output = make_shared<DabOutputUdp>();
#endif // defined(HAVE_OUTPUT_UDP)
#if defined(HAVE_OUTPUT_TCP)
                } else if (proto == "tcp") {
                    output = make_shared<DabOutputTcp>();
#endif // defined(HAVE_OUTPUT_TCP)
#if defined(HAVE_OUTPUT_SIMUL)
                } else if (proto == "simul") {
                    output = make_shared<DabOutputSimul>();
#endif // defined(HAVE_OUTPUT_SIMUL)
#if defined(HAVE_OUTPUT_ZEROMQ)
                /* The legacy configuration setting will not enable metadata,
                 * to keep backward compatibility
                 */
                } else if (proto == "zmq+tcp") {
                    output = make_shared<DabOutputZMQ>("tcp", false);
                } else if (proto == "zmq+ipc") {
                    output = make_shared<DabOutputZMQ>("ipc", false);
                } else if (proto == "zmq+pgm") {
                    output = make_shared<DabOutputZMQ>("pgm", false);
                } else if (proto == "zmq+epgm") {
                    output = make_shared<DabOutputZMQ>("epgm", false);
#endif // defined(HAVE_OUTPUT_ZEROMQ)
                } else {
                    etiLog.level(error) << "Output protocol unknown: " << proto;
                    throw MuxInitException();
                }

                if (not output) {
                    etiLog.level(error) <<
                        "Unable to init output " <<
                        uri;
                    return -1;
                }

                if (output->Open(location) == -1) {
                    etiLog.level(error) <<
                        "Unable to open output " <<
                        uri;
                    return -1;
                }

                outputs.push_back(output);

            }
        }


        if (outputs.size() == 0) {
            etiLog.log(alert, "no output defined");
            throw MuxInitException();
        }

        mux.prepare(output_require_tai_clock);
        mux.print_info();

        etiLog.log(info, "--- Output list ---");
        printOutputs(outputs);

        if (edi_conf.enabled()) {
            edi_conf.print();
        }

        size_t limit = pt.get("general.nbframes", 0);

        etiLog.level(info) << "Start loop";
        /*   Each iteration of the main loop creates one ETI frame */
        size_t currentFrame;
        for (currentFrame = 0; running; currentFrame++) {
            mux.mux_frame(outputs);

            if (limit && currentFrame >= limit) {
                break;
            }

            /* Check every six seconds if the remote control is still working */
            if (currentFrame % 250 == 249) {
                rcs.check_faults();
            }

            /* Same for statistics server */
            if (currentFrame % 10 == 0) {
                ManagementServer& mgmt_server = get_mgmt_server();

                if (mgmt_server.fault_detected()) {
                    etiLog.level(warn) <<
                        "Detected Management Server fault, restarting it";
                    mgmt_server.restart();
                }

                mgmt_server.update_ptree(pt);
            }
        }

        if (limit) {
            etiLog.level(info) << "Max number of ETI frames reached: " << currentFrame;
        }
    }
    catch (const MuxInitException& except) {
        etiLog.level(error) << "Multiplex initialisation aborted: " <<
            except.what();
        returnCode = 1;
    }
    catch (const std::invalid_argument& except) {
        etiLog.level(error) << "Caught invalid argument : " << except.what();
        returnCode = 1;
    }
    catch (const std::out_of_range& except) {
        etiLog.level(error) << "Caught out of range exception : " << except.what();
        returnCode = 1;
    }
    catch (const std::logic_error& except) {
        etiLog.level(error) << "Caught logic error : " << except.what();
        returnCode = 2;
    }
    catch (const std::runtime_error& except) {
        etiLog.level(error) << "Caught runtime error : " << except.what();
        returnCode = 2;
    }

    etiLog.log(debug, "exiting...\n");
    fflush(stderr);

    outputs.clear();

    if (returnCode != 0) {
        etiLog.log(alert, "...aborting\n");
    } else {
        etiLog.log(debug, "...done\n");
    }

    return returnCode;
}

