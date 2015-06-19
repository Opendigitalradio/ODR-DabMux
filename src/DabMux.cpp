/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2014, 2015
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

#include <boost/shared_ptr.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/info_parser.hpp>
#include <cstdio>
#include <stdlib.h>
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

#   include <net/if_packet.h>
#endif

#include <time.h>

#ifdef _WIN32
#   pragma warning ( disable : 4103 )
#   include "Eti.h"
#   pragma warning ( default : 4103 )
#else
#   include "Eti.h"
#endif

#include "dabInputFile.h"
#include "dabInputFifo.h"
#include "dabInputMpegFile.h"
#include "dabInputMpegFifo.h"
#include "dabInputDabplusFile.h"
#include "dabInputDabplusFifo.h"
#include "dabInputPacketFile.h"
#include "dabInputEnhancedPacketFile.h"
#include "dabInputEnhancedFifo.h"
#include "dabInputUdp.h"
#include "dabInputBridgeUdp.h"
#include "dabInputSlip.h"
#include "dabInputTest.h"
#include "dabInputPrbs.h"
#include "dabInputRawFile.h"
#include "dabInputRawFifo.h"
#include "dabInputDmbFile.h"
#include "dabInputDmbUdp.h"


#include "dabOutput/dabOutput.h"
#include "dabOutput/edi/TagItems.h"
#include "dabOutput/edi/TagPacket.h"
#include "dabOutput/edi/AFPacket.h"
#include "dabOutput/edi/PFT.h"
#include "crc.h"
#include "UdpSocket.h"
#include "InetAddress.h"
#include "dabUtils.h"
#include "PcDebug.h"
#include "DabMux.h"
#include "MuxElements.h"
#include "utils.h"
#include "ParserCmdline.h"
#include "ConfigParser.h"
#include "ManagementServer.h"
#include "Log.h"
#include "RemoteControl.h"

using namespace std;
using boost::property_tree::ptree;
using boost::property_tree::ptree_error;



volatile sig_atomic_t running = 1;

void signalHandler(int signum)
{
#ifdef _WIN32
    etiLog.log(debug, "\npid: %i\n", _getpid());
#else
    etiLog.log(debug, "\npid: %i, ppid: %i\n", getpid(), getppid());
#endif
    etiLog.log(debug, "Signal handler called with signal ");
    switch (signum) {
#ifndef _WIN32
    case SIGHUP:
        etiLog.log(debug, "SIGHUP\n");
        break;
    case SIGQUIT:
        etiLog.log(debug, "SIGQUIT\n");
        break;
    case SIGPIPE:
        etiLog.log(debug, "SIGPIPE\n");
        return;
        break;
#endif
    case SIGINT:
        etiLog.log(debug, "SIGINT\n");
        break;
    case SIGTERM:
        etiLog.log(debug, "SIGTERM\n");
        etiLog.log(debug, "Exiting software\n");
        exit(0);
        break;
    default:
        etiLog.log(debug, "number %i\n", signum);
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
    for (int i = 0; i < 4; i++) {
        if (sigaction(sigs[i], &sa, NULL) == -1) {
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
    std::vector<boost::shared_ptr<DabOutput> > outputs;

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
#if ENABLE_CMDLINE_OPTIONS
        else {
            if (!parse_cmdline(argv, argc, outputs, ensemble, &enableTist, &FICL,
                        &factumAnalyzer, &limit)) {
                throw MuxInitException();
            }
        }
#else
        else {
            throw MuxInitException("No configuration file specified");
        }
#endif

        int mgmtserverport = pt.get<int>("general.managementport",
                             pt.get<int>("general.statsserverport", 0) );

        /* Management: stats and config server */
        get_mgmt_server().open(mgmtserverport);

        /************** READ REMOTE CONTROL PARAMETERS *************/
        int telnetport = pt.get<int>("remotecontrol.telnetport", 0);

        boost::shared_ptr<BaseRemoteController> rc;

        if (telnetport != 0) {
            rc = boost::shared_ptr<RemoteControllerTelnet>(
                    new RemoteControllerTelnet(telnetport));
        }
        else {
            rc = boost::shared_ptr<RemoteControllerDummy>(
                    new RemoteControllerDummy());
        }

        DabMultiplexer mux(rc, pt);

        etiLog.level(info) <<
                PACKAGE_NAME << " " <<
#if defined(GITVERSION)
                GITVERSION <<
#else
                PACKAGE_VERSION <<
#endif
                " starting up";


        /******************** READ OUTPUT PARAMETERS ***************/
        set<string> all_output_names;
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
#if ENABLE_EDI
                ptree pt_edi = pt_outputs.get_child("edi");

                edi->enabled     = true;

                edi->dest_addr   = pt_edi.get<string>("destination");
                edi->dest_port   = pt_edi.get<unsigned int>("port");
                edi->source_port = pt_edi.get<unsigned int>("sourceport");

                edi->dump        = pt_edi.get<bool>("dump");
                edi->enable_pft  = pt_edi.get<bool>("enable_pft");
                edi->verbose     = pt_edi.get<bool>("verbose");
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

                DabOutput *output;

                if (0) {
#if defined(HAVE_OUTPUT_FILE)
                } else if (proto == "file") {
                    output = new DabOutputFile();
#endif // defined(HAVE_OUTPUT_FILE)
#if defined(HAVE_OUTPUT_FIFO)
                } else if (proto == "fifo") {
                    output = new DabOutputFifo();
#endif // !defined(HAVE_OUTPUT_FIFO)
#if defined(HAVE_OUTPUT_RAW)
                } else if (proto == "raw") {
                    output = new DabOutputRaw();
#endif // defined(HAVE_OUTPUT_RAW)
#if defined(HAVE_OUTPUT_UDP)
                } else if (proto == "udp") {
                    output = new DabOutputUdp();
#endif // defined(HAVE_OUTPUT_UDP)
#if defined(HAVE_OUTPUT_TCP)
                } else if (proto == "tcp") {
                    output = new DabOutputTcp();
#endif // defined(HAVE_OUTPUT_TCP)
#if defined(HAVE_OUTPUT_SIMUL)
                } else if (proto == "simul") {
                    output = new DabOutputSimul();
#endif // defined(HAVE_OUTPUT_SIMUL)
#if defined(HAVE_OUTPUT_ZEROMQ)
                } else if (proto == "zmq+tcp") {
                    output = new DabOutputZMQ("tcp");
                } else if (proto == "zmq+ipc") {
                    output = new DabOutputZMQ("ipc");
                } else if (proto == "zmq+pgm") {
                    output = new DabOutputZMQ("pgm");
                } else if (proto == "zmq+epgm") {
                    output = new DabOutputZMQ("epgm");
#endif // defined(HAVE_OUTPUT_ZEROMQ)
                } else {
                    etiLog.level(error) << "Output protocol unknown: " << proto;
                    throw MuxInitException();
                }

                if (output == NULL) {
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

                boost::shared_ptr<DabOutput> dabout(output);
                outputs.push_back(dabout);

            }
        }


        if (outputs.size() == 0) {
            etiLog.log(emerg, "no output defined");
            throw MuxInitException();
        }

        mux.prepare();
        mux.print_info();

        etiLog.log(info, "--- Output list ---");
        printOutputs(outputs);

#if ENABLE_EDI
        if (edi_conf.enabled) {
            etiLog.level(warn) << "EXPERIMENTAL EDI OUTPUT ENABLED!";
            etiLog.level(info) << "edi to " << edi_conf.dest_addr << ":" << edi_conf.dest_port;
            etiLog.level(info) << "source port " << edi_conf.source_port;
            etiLog.level(info) << "verbose     " << edi_conf.verbose;
        }


        if (edi_conf.verbose) {
            etiLog.log(info, "Setup EDI debug");
        }
        std::ofstream edi_debug_file;

        if (edi_conf.dump) {
            edi_debug_file.open("./edi.debug");
        }
        UdpSocket edi_output;

        if (edi_conf.enabled) {
            edi_output.create(edi_conf.source_port);
        }

        if (edi_conf.verbose) {
            etiLog.log(info, "EDI debug set up");
        }

        // The TagPacket will then be placed into an AFPacket
        AFPacketiser edi_afPacketiser(edi_conf.verbose);

        // The AF Packet will be protected with reed-solomon and split in fragments
        PFT edi_pft(207, 3, edi_conf);
#endif

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
            if ((currentFrame % 250 == 249) && rc->fault_detected()) {
                etiLog.level(warn) << "Detected Remote Control fault, restarting it";
                rc->restart();
            }

            /* Same for statistics server */
            if (currentFrame % 10 == 0) {
                ManagementServer& mgmt_server = get_mgmt_server();

                if (mgmt_server.fault_detected()) {
                    etiLog.level(warn) <<
                        "Detected Management Server fault, restarting it";
                    mgmt_server.restart();
                }
                else if (mgmt_server.request_pending()) {
                    mgmt_server.update_ptree(pt);
                }
                /* else if (mgmt_server.retrieve_new_ptree(pt)) {
                   }
                   */
            }
        }
        etiLog.level(info) << "Goodbye";
    }
    catch (const MuxInitException& except) {
        etiLog.level(error) << "Multiplex initialisation aborted: " <<
            except.what();
    }
    catch (const std::invalid_argument& except) {
        etiLog.level(error) << "Caught invalid argument : " << except.what();
    }
    catch (const std::runtime_error& except) {
        etiLog.level(error) << "Caught runtime error : " << except.what();
    }

    etiLog.log(debug, "exiting...\n");
    fflush(stderr);

    outputs.clear();

    UdpSocket::clean();

    if (returnCode < 0) {
        etiLog.log(emerg, "...aborting\n");
    } else {
        etiLog.log(debug, "...done\n");
    }

    return returnCode;
}

