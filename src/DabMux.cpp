/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2022
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

#include <memory>
#include <boost/property_tree/ptree.hpp>
#include <ctime>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <fcntl.h>
#include <signal.h>

#include <vector>
#include <set>

#include "DabMultiplexer.h"

#include "dabOutput/dabOutput.h"
#include "MuxElements.h"
#include "utils.h"
#include "ManagementServer.h"
#include "Log.h"
#include "RemoteControl.h"

using namespace std;
using boost::property_tree::ptree;

volatile sig_atomic_t running = 1;

/* We are not allowed to use etiLog in the signal handler,
 * because etiLog uses mutexes
 */
void signalHandler(int signum)
{
    fprintf(stderr, "\npid: %i, ppid: %i\n", getpid(), getppid());

#define SIG_MSG "Signal received: "
    switch (signum) {
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
    killpg(0, SIGPIPE);
    running = 0;
}


int main(int argc, char *argv[])
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

    // Use the lowest real-time priority for this thread, and switch to real-time scheduling
    const int policy = SCHED_RR;
    sched_param sp;
    sp.sched_priority = sched_get_priority_min(policy);
    int thread_prio_ret = pthread_setschedparam(pthread_self(), policy, &sp);
    if (thread_prio_ret != 0) {
        etiLog.level(error) << "Could not set real-time priority for thread:" << thread_prio_ret;
    }

    int returnCode = 0;
    std::vector<std::shared_ptr<DabOutput> > outputs;

    try {
        string conf_file = "";

        DabMultiplexerConfig mux_conf;

        if (argc == 2) { // Assume the only argument is a config file
            conf_file = argv[1];

            if (conf_file == "-h") {
                printUsage(argv[0], stdout);
                throw MuxInitException("Nothing to do");
            }
        }
        else if (argc > 1 && strncmp(argv[1], "-e", 2) == 0) { // use external config file
            try {
                if (argc != 3) {
                    printUsage(argv[0], stderr);
                    throw MuxInitException();
                }

                conf_file = argv[2];
                mux_conf.read(conf_file);
            }
            catch (runtime_error &e) {
                throw MuxInitException(e.what());
            }
        }

        if (conf_file.empty()) {
            printUsage(argv[0], stderr);
            throw MuxInitException("No configuration file specified");
        }

        try {
            mux_conf.read(conf_file);
        }
        catch (runtime_error &e) {
            throw MuxInitException(e.what());
        }

        /* Enable Logging to syslog conditionally */
        if (mux_conf.pt.get<bool>("general.syslog", false)) {
            etiLog.register_backend(std::make_shared<LogToSyslog>());
        }

        const auto startupcheck = mux_conf.pt.get<string>("general.startupcheck", "");
        if (not startupcheck.empty()) {
            etiLog.level(info) << "Running startup check '" << startupcheck << "'";
            int wstatus = system(startupcheck.c_str());

            if (WIFEXITED(wstatus)) {
                if (WEXITSTATUS(wstatus) == 0) {
                    etiLog.level(info) << "Startup check ok";
                }
                else {
                    etiLog.level(error) << "Startup check failed, returned " << WEXITSTATUS(wstatus);
                    return 1;
                }
            }
            else {
                etiLog.level(error) << "Startup check failed, child didn't terminate normally";
                return 1;
            }
        }

        int mgmtserverport = mux_conf.pt.get<int>("general.managementport",
                             mux_conf.pt.get<int>("general.statsserverport", 0) );

        /* Management: stats and config server */
        get_mgmt_server().open(mgmtserverport);

        /************** READ REMOTE CONTROL PARAMETERS *************/
        int telnetport = mux_conf.pt.get<int>("remotecontrol.telnetport", 0);
        if (telnetport != 0) {
            auto rc = std::make_shared<RemoteControllerTelnet>(telnetport);
            rcs.add_controller(rc);
        }

        auto zmqendpoint = mux_conf.pt.get<string>("remotecontrol.zmqendpoint", "");
        if (not zmqendpoint.empty()) {
            auto rc = std::make_shared<RemoteControllerZmq>(zmqendpoint);
            rcs.add_controller(rc);
        }

        DabMultiplexer mux(mux_conf);

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
        ptree pt_outputs = mux_conf.pt.get_child("outputs");
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

                bool default_enable_pft = pt_edi.get<bool>("enable_pft", false);
                edi_conf.verbose = pt_edi.get<bool>("verbose", false);

                unsigned int default_fec = pt_edi.get<unsigned int>("fec", 3);
                unsigned int default_chunk_len = pt_edi.get<unsigned int>("chunk_len", 207);

                auto check_spreading_factor = [](int percent) {
                    if (percent < 0) {
                        throw std::runtime_error("EDI output: negative packet_spread value is invalid.");
                    }
                    double factor = (double)percent / 100.0;
                    if (factor > 30000) {
                        throw std::runtime_error("EDI output: interleaving set for more than 30 seconds!");
                    }
                    return factor;
                };

                double default_spreading_factor = check_spreading_factor(pt_edi.get<int>("packet_spread", 95));

                using pt_t = boost::property_tree::basic_ptree<std::basic_string<char>, std::basic_string<char>>;
                auto handle_overrides = [&](edi::pft_settings_t& pft_settings, pt_t pt) {
                    pft_settings.chunk_len = pt.get<unsigned int>("chunk_len", default_chunk_len);
                    pft_settings.enable_pft = pt.get<bool>("enable_pft", default_enable_pft);
                    pft_settings.fec = pt.get<unsigned int>("fec", default_fec);
                    pft_settings.fragment_spreading_factor = default_spreading_factor;
                    if (auto override_spread_percent = pt.get_optional<int>("packet_spread"))
                        pft_settings.fragment_spreading_factor = check_spreading_factor(*override_spread_percent);

                    pft_settings.verbose = pt.get<bool>("verbose", edi_conf.verbose);
                };

                for (auto pt_edi_dest : pt_edi.get_child("destinations")) {
                    const auto proto = pt_edi_dest.second.get<string>("protocol", "udp");
                    if (proto == "udp") {
                        auto dest = make_shared<edi::udp_destination_t>();
                        dest->dest_addr   = pt_edi_dest.second.get<string>("destination");
                        if (auto ttl = pt_edi_dest.second.get_optional<unsigned int>("ttl"))
                            dest->ttl = *ttl;

                        dest->source_addr = pt_edi_dest.second.get<string>("source", "");
                        dest->source_port = pt_edi_dest.second.get<unsigned int>("sourceport", 0);
                        dest->dest_port   = pt_edi_dest.second.get<unsigned int>("port", 0);
                        if (dest->dest_port == 0) {
                            // Compatiblity: we have removed the transport and addressing in the
                            // PFT layer, which removed the requirement that all outputs must share
                            // the same destination port. If missing from the destination specification,
                            // we read it from the parent block, where it was before.
                            dest->dest_port       = pt_edi.get<unsigned int>("port");
                        }

                        handle_overrides(dest->pft_settings, pt_edi_dest.second);

                        edi_conf.destinations.push_back(dest);
                    }
                    else if (proto == "tcp") {
                        auto dest = make_shared<edi::tcp_server_t>();
                        dest->listen_port = pt_edi_dest.second.get<unsigned int>("listenport");
                        dest->max_frames_queued = pt_edi_dest.second.get<size_t>("max_frames_queued", 500);
                        double preroll = pt_edi_dest.second.get<double>("preroll-burst", 0.0);
                        dest->tcp_server_preroll_buffers = ceil(preroll / 24e-3);

                        handle_overrides(dest->pft_settings, pt_edi_dest.second);

                        edi_conf.destinations.push_back(dest);
                    }
                    else {
                        throw runtime_error("Unknown EDI protocol " + proto);
                    }
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

        const size_t limit = mux_conf.pt.get("general.nbframes", 0);

        etiLog.level(info) << "Start loop";
        /*   Each iteration of the main loop creates one ETI frame */
        size_t currentFrame;
        for (currentFrame = 0; running; currentFrame++) {
            mux.mux_frame(outputs);

            if (limit && currentFrame >= limit) {
                etiLog.level(info) << "Max number of ETI frames reached: " << currentFrame;
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

                mgmt_server.update_ptree(mux_conf.pt);
            }
        }
    }
    catch (const MuxInitException& except) {
        etiLog.level(error) << "Multiplex initialisation aborted: " << except.what();
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

