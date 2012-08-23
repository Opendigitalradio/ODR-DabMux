/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Includes modifications
   2012, Matthias P. Braendli, matthias.braendli@mpb.li

   The command-line parser reads the parameters given on the command
   line, and builds an ensemble.
   */
/*
   This file is part of CRC-DabMux.

   CRC-DabMux is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   CRC-DabMux is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with CRC-DabMux.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "ParserCmdline.h"

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include <vector>
#include <stdint.h>
#include <cstring>
#include "dabOutput.h"
#include "dabInput.h"
#include "utils.h"
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
#include "DabMux.h"


#ifdef _WIN32
#   pragma warning ( disable : 4103 )
#   include "Eti.h"
#   pragma warning ( default : 4103 )
#else
#   include "Eti.h"
#endif


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
#   include <unistd.h>
#   include <sys/time.h>
#   include <sys/wait.h>
#   include <sys/ioctl.h>
#   include <sys/times.h>
#endif

bool parse_cmdline(char **argv,
        int argc,
        vector<dabOutput*> &outputs,
        dabEnsemble* ensemble,
        bool* enableTist,
        unsigned* FICL,
        bool* factumAnalyzer,
        unsigned long* limit
        )
{
    vector<dabOutput*>::iterator output;
    vector<dabSubchannel*>::iterator subchannel = ensemble->subchannels.end();
    vector<dabComponent*>::iterator component = ensemble->components.end();
    vector<dabService*>::iterator service = ensemble->services.end();
    dabProtection* protection = NULL;

    int scids_temp = 0;

    char* progName = strrchr(argv[0], '/');
    if (progName == NULL) {
        progName = argv[0];
    } else {
        ++progName;
    }

    while (1) {
        int c = getopt(argc, argv,
                "A:B:CD:E:F:L:M:O:P:STVa:b:c:de:f:g:hi:kl:m:n:op:rst:y:z");
        if (c == -1) {
            break;
        }
        switch (c) {
        case 'O':
            outputs.push_back(new dabOutput);
            output = outputs.end() - 1;

            memset(*output, 0, sizeof(dabOutput));
            (*output)->outputProto = NULL;
            (*output)->outputName = NULL;
            (*output)->data = NULL;
            (*output)->operations = dabOutputDefaultOperations;

            char* proto;

            proto = strstr(optarg, "://");
            if (proto == NULL) {
                etiLog.printHeader(TcpLog::ERR,
                        "No protocol defined for output\n");
                goto EXIT;
            } else {
                (*output)->outputProto = optarg;
                (*output)->outputName = proto + 3;
                *proto = 0;
            }
            subchannel = ensemble->subchannels.end();
            protection = NULL;
            component = ensemble->components.end();
            service = ensemble->services.end();
            break;
        case 'S':
            ensemble->services.push_back(new dabService);

            subchannel = ensemble->subchannels.end();
            protection = NULL;
            component = ensemble->components.end();
            service = ensemble->services.end() - 1;
            output = outputs.end();

            memset((*service)->label.text, 0, 16);
            sprintf((*service)->label.text, "CRC-Service%i",
                    (int)ensemble->services.size());
            (*service)->label.flag = 0xe01f;
            (*service)->id = DEFAULT_SERVICE_ID + ensemble->services.size();
            (*service)->pty = 0;
            (*service)->language = 0;
            scids_temp = 0;

            break;
        case 'C':
            if (service == ensemble->services.end()) {
                etiLog.printHeader(TcpLog::ERR, "You must define a service"
                        " before using option -%c\n", c);
                printUsage(progName);
                goto EXIT;
            }

            ensemble->components.push_back(new dabComponent);

            component = ensemble->components.end() - 1;
            subchannel = ensemble->subchannels.end();
            protection = NULL;
            output = outputs.end();

            memset(*component, 0, sizeof(dabComponent));
            memset((*component)->label.text, 0, 16);
            (*component)->label.flag = 0xffff;
            (*component)->serviceId = (*service)->id;
            (*component)->subchId = (*(ensemble->subchannels.end() - 1))->id;
            (*component)->SCIdS = scids_temp++;
            break;
        case 'A':
        case 'B':
        case 'D':
        case 'E':
        case 'F':
        case 'M':
        case 'P':
        case 'T':
            if (optarg == NULL && c != 'T') {
                etiLog.printHeader(TcpLog::ERR,
                        "Missing parameter for option -%c\n", c);
                printUsage(progName);
                goto EXIT;
            }

            ensemble->subchannels.push_back(new dabSubchannel);

            subchannel = ensemble->subchannels.end() - 1;
            protection = &(*subchannel)->protection;
            component = ensemble->components.end();
            service = ensemble->services.end();
            output = outputs.end();

            if (c != 'T') {
                (*subchannel)->inputName = optarg;
            } else {
                (*subchannel)->inputName = NULL;
            }
            if (0) {
#if defined(HAVE_INPUT_FILE) && defined(HAVE_FORMAT_MPEG)
            } else if (c == 'A') {
                (*subchannel)->inputProto = "file";
                (*subchannel)->type = 0;
                (*subchannel)->bitrate = 0;
                (*subchannel)->operations = dabInputMpegFileOperations;
#endif // defined(HAVE_INPUT_FILE) && defined(HAVE_FORMAT_MPEG)
#if defined(HAVE_FORMAT_DABPLUS)
            } else if (c == 'F') {
                (*subchannel)->type = 0;
                (*subchannel)->bitrate = 32;

                char* proto;

                proto = strstr(optarg, "://");
                if (proto == NULL) {
                    (*subchannel)->inputProto = "file";
                } else {
                    (*subchannel)->inputProto = optarg;
                    (*subchannel)->inputName = proto + 3;
                    *proto = 0;
                }

                if (0) {
#if defined(HAVE_INPUT_FILE)
                } else if (strcmp((*subchannel)->inputProto, "file") == 0) {
                    (*subchannel)->operations = dabInputDabplusFileOperations;
#endif // defined(HAVE_INPUT_FILE)
                } else {
                    etiLog.printHeader(TcpLog::ERR,
                            "Invalid protocol for DAB+ input (%s)\n",
                            (*subchannel)->inputProto);
                    printUsage(progName);
                    goto EXIT;
                }
#endif // defined(HAVE_FORMAT_DABPLUS)
            } else if (c == 'B') {
                char* proto;

                proto = strstr(optarg, "://");
                if (proto == NULL) {
                    (*subchannel)->inputProto = "udp";
                } else {
                    (*subchannel)->inputProto = optarg;
                    (*subchannel)->inputName = proto + 3;
                    *proto = 0;
                }
                if (0) {
#if defined(HAVE_FORMAT_BRIDGE)
#if defined(HAVE_INPUT_UDP)
                } else if (strcmp((*subchannel)->inputProto, "udp") == 0) {
                    (*subchannel)->operations = dabInputBridgeUdpOperations;
#endif // defined(HAVE_INPUT_UDP)
#if defined(HAVE_INPUT_SLIP)
                } else if (strcmp((*subchannel)->inputProto, "slip") == 0) {
                    (*subchannel)->operations = dabInputSlipOperations;
#endif // defined(HAVE_INPUT_SLIP)
#endif // defined(HAVE_FORMAT_BRIDGE)
                }
            } else if (c == 'D') {
                char* proto;

                proto = strstr(optarg, "://");
                if (proto == NULL) {
                    (*subchannel)->inputProto = "udp";
                } else {
                    (*subchannel)->inputProto = optarg;
                    (*subchannel)->inputName = proto + 3;
                    *proto = 0;
                }
                if (0) {
#if defined(HAVE_INPUT_UDP)
                } else if (strcmp((*subchannel)->inputProto, "udp") == 0) {
                    (*subchannel)->operations = dabInputUdpOperations;
#endif
#if defined(HAVE_INPUT_PRBS) && defined(HAVE_FORMAT_RAW)
                } else if (strcmp((*subchannel)->inputProto, "prbs") == 0) {
                    (*subchannel)->operations = dabInputPrbsOperations;
#endif
#if defined(HAVE_INPUT_FILE) && defined(HAVE_FORMAT_RAW)
                } else if (strcmp((*subchannel)->inputProto, "file") == 0) {
                    (*subchannel)->operations = dabInputRawFileOperations;
#endif
                } else if (strcmp((*subchannel)->inputProto, "fifo") == 0) {
                    (*subchannel)->operations = dabInputRawFifoOperations;
                } else {
                    etiLog.printHeader(TcpLog::ERR,
                            "Invalid protocol for data input (%s)\n",
                            (*subchannel)->inputProto);
                    printUsage(progName);
                    goto EXIT;
                }

                (*subchannel)->type = 1;
                (*subchannel)->bitrate = DEFAULT_DATA_BITRATE;
#if defined(HAVE_INPUT_TEST) && defined(HAVE_FORMAT_RAW)
            } else if (c == 'T') {
                (*subchannel)->inputProto = "test";
                (*subchannel)->type = 1;
                (*subchannel)->bitrate = DEFAULT_DATA_BITRATE;
                (*subchannel)->operations = dabInputTestOperations;
#endif // defined(HAVE_INPUT_TEST)) && defined(HAVE_FORMAT_RAW)
#ifdef HAVE_FORMAT_PACKET
            } else if (c == 'P') {
                (*subchannel)->inputProto = "file";
                (*subchannel)->type = 3;
                (*subchannel)->bitrate = DEFAULT_PACKET_BITRATE;
#ifdef HAVE_INPUT_FILE
                (*subchannel)->operations = dabInputPacketFileOperations;
#elif defined(HAVE_INPUT_FIFO)
                (*subchannel)->operations = dabInputFifoOperations;
#else
#   pragma error("Must defined at least one packet input")
#endif // defined(HAVE_INPUT_FILE)
#ifdef HAVE_FORMAT_EPM
            } else if (c == 'E') {
                (*subchannel)->inputProto = "file";
                (*subchannel)->type = 3;
                (*subchannel)->bitrate = DEFAULT_PACKET_BITRATE;
                (*subchannel)->operations = dabInputEnhancedPacketFileOperations;
#endif // defined(HAVE_FORMAT_EPM)
#endif // defined(HAVE_FORMAT_PACKET)
#ifdef HAVE_FORMAT_DMB
            } else if (c == 'M') {
                char* proto;

                proto = strstr(optarg, "://");
                if (proto == NULL) {
                    (*subchannel)->inputProto = "udp";
                } else {
                    (*subchannel)->inputProto = optarg;
                    (*subchannel)->inputName = proto + 3;
                    *proto = 0;
                }
                if (strcmp((*subchannel)->inputProto, "udp") == 0) {
                    (*subchannel)->operations = dabInputDmbUdpOperations;
                } else if (strcmp((*subchannel)->inputProto, "file") == 0) {
                    (*subchannel)->operations = dabInputDmbFileOperations;
                } else {
                    etiLog.printHeader(TcpLog::ERR,
                            "Invalid protocol for DMB input (%s)\n",
                            (*subchannel)->inputProto);
                    printUsage(progName);
                    goto EXIT;
                }

                (*subchannel)->type = 1;
                (*subchannel)->bitrate = DEFAULT_DATA_BITRATE;
#endif
            } else {
                etiLog.printHeader(TcpLog::ERR,
                        "Service '%c' not yet coded!\n", c);
                goto EXIT;
            }
            (*subchannel)->operations.init(&(*subchannel)->data);
            for (int i = 0; i < 64; ++i) { // Find first free subchannel
                subchannel = getSubchannel(ensemble->subchannels, i);
                if (subchannel == ensemble->subchannels.end()) {
                    subchannel = ensemble->subchannels.end() - 1;
                    (*subchannel)->id = i;
                    break;
                }
            }
            (*subchannel)->startAddress = 0;

            if (c == 'A') {
                protection->form = 0;
                protection->level = 2;
                protection->shortForm.tableSwitch = 0;
                protection->shortForm.tableIndex = 0;
            } else {
                protection->level = 2;
                protection->form = 1;
                protection->longForm.option = 0;
            }
            break;
        case 'L':
            if (optarg == NULL) {
                etiLog.printHeader(TcpLog::ERR,
                        "Missing parameter for option -L\n");
                printUsage(progName);
                goto EXIT;
            }
            if (service == ensemble->services.end()) {
                memset(ensemble->label.text, 0, 16);
                strncpy(ensemble->label.text, optarg, 16);
                ensemble->label.flag = 0xff00;
            } else if (component != ensemble->components.end()) {
                memset((*component)->label.text, 0, 16);
                strncpy((*component)->label.text, optarg, 16);
                (*component)->label.flag = 0xff00;
            } else {    // Service
                memset((*service)->label.text, 0, 16);
                strncpy((*service)->label.text, optarg, 16);
                (*service)->label.flag = 0xff00;
            }
            // TODO Check strlen before doing short label
            // TODO Check if short label already set
            break;
        case 'V':
            goto EXIT;
        case 'l':
            if (optarg == NULL) {
                etiLog.printHeader(TcpLog::ERR,
                        "Missing parameter for option -l\n");
                printUsage(progName);
                goto EXIT;
            }
            if (service == ensemble->services.end()) {
                char* end;
                ensemble->label.flag = strtoul(optarg, &end, 0);
                if (*end != 0) {
                    end = optarg;
                    ensemble->label.flag = 0;
                    for (int i = 0; i < 32; ++i) {
                        if (*end == ensemble->label.text[i]) {
                            ensemble->label.flag |= 0x8000 >> i;
                            if (*(++end) == 0) {
                                break;
                            }
                        }
                    }
                    if (*end != 0) {
                        etiLog.printHeader(TcpLog::ERR,
                                "Error at '%c' in ensemble short label '%s'!\n"
                                "Not in label '%s'!\n",
                                *end, optarg, ensemble->label.text);
                        goto EXIT;
                    }
                }
                int count = 0;
                for (int i = 0; i < 16; ++i) {
                    if (ensemble->label.flag & (1 << i)) {
                        ++count;
                    }
                }
                if (count > 8) {
                    etiLog.printHeader(TcpLog::ERR,
                            "Ensemble short label too long!\n"
                            "Must be < 8 characters.\n");
                    goto EXIT;
                }
            } else if (component != ensemble->components.end()) {
                char* end;
                (*component)->label.flag = strtoul(optarg, &end, 0);
                if (*end != 0) {
                    end = optarg;
                    (*component)->label.flag = 0;
                    for (int i = 0; i < 32; ++i) {
                        if (*end == (*component)->label.text[i]) {
                            (*component)->label.flag |= 0x8000 >> i;
                            if (*(++end) == 0) {
                                break;
                            }
                        }
                    }
                    if (*end != 0) {
                        etiLog.printHeader(TcpLog::ERR,
                                "Error at '%c' in component short label '%s'!\n"
                                "Not in label '%s'!\n",
                                *end, optarg, (*component)->label.text);
                        goto EXIT;
                    }
                }
                int count = 0;
                for (int i = 0; i < 16; ++i) {
                    if ((*component)->label.flag & (1 << i)) {
                        ++count;
                    }
                }
                if (count > 8) {
                    etiLog.printHeader(TcpLog::ERR,
                            "Service '%s' short label too long!\n"
                            "Must be < 8 characters.\n", (*component)->label.text);
                    goto EXIT;
                }
            } else {
                char* end;
                (*service)->label.flag = strtoul(optarg, &end, 0);
                if (*end != 0) {
                    end = optarg;
                    (*service)->label.flag = 0;
                    for (int i = 0; i < 32; ++i) {
                        if (*end == (*service)->label.text[i]) {
                            (*service)->label.flag |= 0x8000 >> i;
                            if (*(++end) == 0) {
                                break;
                            }
                        }
                    }
                    if (*end != 0) {
                        etiLog.printHeader(TcpLog::ERR,
                                "Error at '%c' in service short label '%s'!\n"
                                "Not in label '%s'!\n",
                                *end, optarg, (*service)->label.text);
                        goto EXIT;
                    }
                }
                int count = 0;
                for (int i = 0; i < 16; ++i) {
                    if ((*service)->label.flag & (1 << i)) {
                        ++count;
                    }
                }
                if (count > 8) {
                    etiLog.printHeader(TcpLog::ERR,
                            "Service '%s' short label too long!\n"
                            "Must be < 8 characters.\n", (*service)->label.text);
                    goto EXIT;
                }
            }
            break;
        case 'i':
            if (optarg == NULL) {
                etiLog.printHeader(TcpLog::ERR,
                        "Missing parameter for option -i\n");
                printUsage(progName);
                goto EXIT;
            }
            if (component != ensemble->components.end()) {
                (*component)->subchId = strtoul(optarg, NULL, 0);
            } else if (subchannel != ensemble->subchannels.end()) {
                (*subchannel)->id = strtoul(optarg, NULL, 0);
            } else if (service != ensemble->services.end()) {
                (*service)->id = strtoul(optarg, NULL, 0);
                if ((*service)->id == 0) {
                    etiLog.printHeader(TcpLog::ERR,
                            "Service id 0 is invalid\n");
                    goto EXIT;
                }
            } else {
                ensemble->id = strtoul(optarg, NULL, 0);
            }
            break;
        case 'b':
            if (optarg == NULL) {
                etiLog.printHeader(TcpLog::ERR,
                        "Missing parameter for option -b\n");
                printUsage(progName);
                goto EXIT;
            }
            if (subchannel == ensemble->subchannels.end()) {
                etiLog.printHeader(TcpLog::ERR,
                        "You must define a subchannel first!\n");
                printUsage(progName);
                goto EXIT;
            }
            (*subchannel)->bitrate = strtoul(optarg, NULL, 0);
            if (((*subchannel)->bitrate & 0x7) != 0) {
                (*subchannel)->bitrate += 8;
                (*subchannel)->bitrate &= ~0x7;
                etiLog.printHeader(TcpLog::WARNING,
                        "bitrate must be multiple of 8 -> ceiling to %i\n",
                        (*subchannel)->bitrate);
            }
            break;
        case 'c':
            if (optarg == NULL) {
                etiLog.printHeader(TcpLog::ERR,
                        "Missing parameter for option -c\n");
                printUsage(progName);
                goto EXIT;
            }
            ensemble->ecc = strtoul(optarg, NULL, 0);
            break;
#if defined(HAVE_INPUT_FIFO) && defined(HAVE_INPUT_FILE)
        case 'k':
            if (subchannel == ensemble->subchannels.end()) {
                etiLog.printHeader(TcpLog::ERR,
                        "You must define a subchannel first!\n");
                printUsage(progName);
                goto EXIT;
            }
            switch ((*subchannel)->type) {
#ifdef HAVE_FORMAT_PACKET
            case 3:
                (*subchannel)->operations.clean(&(*subchannel)->data);
                if ((*subchannel)->operations == dabInputPacketFileOperations) {
                    (*subchannel)->operations = dabInputFifoOperations;
#ifdef HAVE_FORMAT_EPM
                } else if ((*subchannel)->operations == dabInputEnhancedPacketFileOperations) {
                    (*subchannel)->operations = dabInputEnhancedFifoOperations;
#endif // defined(HAVE_FORMAT_EPM)
                } else {
                    etiLog.printHeader(TcpLog::ERR,
                            "Error, wrong packet subchannel operations!\n");
                    goto EXIT;
                }
                (*subchannel)->operations.init(&(*subchannel)->data);
                (*subchannel)->inputProto = "fifo";
                break;
#endif // defined(HAVE_FORMAT_PACKET)
#ifdef HAVE_FORMAT_MPEG
            case 0:
                (*subchannel)->operations.clean(&(*subchannel)->data);
                if ((*subchannel)->operations == dabInputMpegFileOperations) {
                    (*subchannel)->operations = dabInputMpegFifoOperations;
                } else if ((*subchannel)->operations ==
                        dabInputDabplusFileOperations) {
                    (*subchannel)->operations = dabInputDabplusFifoOperations;
                } else {
                    etiLog.printHeader(TcpLog::ERR,
                            "Error, wrong audio subchannel operations!\n");
                    goto EXIT;
                }
                (*subchannel)->operations.init(&(*subchannel)->data);
                (*subchannel)->inputProto = "fifo";
                break;
#endif // defined(HAVE_FORMAT_MPEG)
            default:
                etiLog.printHeader(TcpLog::ERR,
                        "sorry, non-blocking input file is "
                        "only valid with audio or packet services\n");
                goto EXIT;
            }
            break;
#endif // defined(HAVE_INPUT_FIFO) && defined(HAVE_INPUT_FILE)
        case 'p':
            int level;
            if (optarg == NULL) {
                etiLog.printHeader(TcpLog::ERR,
                        "Missing parameter for option -P\n");
                printUsage(progName);
                goto EXIT;
            }
            if (subchannel == ensemble->subchannels.end()) {
                etiLog.printHeader(TcpLog::ERR,
                        "You must define a subchannel first!\n");
                printUsage(progName);
                goto EXIT;
            }
            level = strtoul(optarg, NULL, 0) - 1;
            if (protection->form == 0) {
                if ((level < 0) || (level > 4)) {
                    etiLog.printHeader(TcpLog::ERR,
                            "protection level must be between "
                            "1 to 5 inclusively (current = %i)\n", level);
                    goto EXIT;
                }
            } else {
                if ((level < 0) || (level > 3)) {
                    etiLog.printHeader(TcpLog::ERR,
                            "protection level must be between "
                            "1 to 4 inclusively (current = %i)\n", level);
                    goto EXIT;
                }
            }
            protection->level = level;
            break;
        case 'm':
            if (optarg) {
                ensemble->mode = strtoul(optarg, NULL, 0);
                if ((ensemble->mode < 1) || (ensemble->mode > 4)) {
                    etiLog.printHeader(TcpLog::ERR,
                            "Mode must be between 1-4\n");
                    goto EXIT;
                }
                if (ensemble->mode == 4)
                    ensemble->mode = 0;
                if  (ensemble->mode == 3) {
                    *FICL = 32;
                } else {
                    *FICL = 24;
                }
            } else {
                etiLog.printHeader(TcpLog::ERR,
                        "Missing parameter for option -m\n");
                printUsage(progName);
                goto EXIT;
            }
            break;
        case 'n':
            if (optarg) {
                *limit = strtoul(optarg, NULL, 0);
            } else {
                etiLog.printHeader(TcpLog::ERR,
                        "Missing parameter for option -n\n");
                printUsage(progName);
                goto EXIT;
            }
            break;
        case 'o':
            etiLog.open("createETI", 0, 12222);
            break;
        case 't':
            if (optarg == NULL) {
                etiLog.printHeader(TcpLog::ERR,
                        "Missing parameter for option -t\n");
                printUsage(progName);
                goto EXIT;
            }
            if (component == ensemble->components.end()) {
                etiLog.printHeader(TcpLog::ERR,
                        "You must define a component before setting "
                        "service type!\n");
                printUsage(progName);
                goto EXIT;
            }
            (*component)->type = strtoul(optarg, NULL, 0);
            break;
        case 'a':
            if (component == ensemble->components.end()) {
                etiLog.printHeader(TcpLog::ERR,
                        "You must define a component before setting "
                        "packet address!\n");
                printUsage(progName);
                goto EXIT;
            }
            if (!(*component)->isPacketComponent(ensemble->subchannels)) {
                etiLog.printHeader(TcpLog::ERR, "address\n");
                printUsage(progName);
                goto EXIT;
            }
            (*component)->packet.address = strtoul(optarg, NULL, 0);
            break;
        case 'd':
            if (component == ensemble->components.end()) {
                etiLog.printHeader(TcpLog::ERR,
                        "You must define a component before setting "
                        "datagroup!\n");
                printUsage(progName);
                goto EXIT;
            }
            if (!(*component)->isPacketComponent(ensemble->subchannels)) {
                etiLog.printHeader(TcpLog::ERR, "datagroup\n");
                printUsage(progName);
                goto EXIT;
            }
            (*component)->packet.datagroup = true;
            break;
        case 'f':
            if (component == ensemble->components.end()) {
                etiLog.printHeader(TcpLog::ERR,
                        "You must define a component first!\n");
                printUsage(progName);
                goto EXIT;
            }
            if (!(*component)->isPacketComponent(ensemble->subchannels)) {
                etiLog.printHeader(TcpLog::ERR, "application type\n");
                printUsage(progName);
                goto EXIT;
            }
            (*component)->packet.appType = strtoul(optarg, NULL, 0);
            break;
        case 'g':
            if (service == ensemble->services.end()) {
                etiLog.printHeader(TcpLog::ERR, "You must define a service"
                        " before using option -%c\n", c);
                printUsage(progName);
                goto EXIT;
            }
            (*service)->language = strtoul(optarg, NULL, 0);
            break;
        case 's':
            {
                /*
                struct timeval tv;
                gettimeofday(&tv, NULL);
                unsigned _8ms = (tv.tv_usec / 1000) / 8;
                unsigned _1ms = (tv.tv_usec - (_8ms * 8000)) / 1000;
                unsigned _4us = 20;
                unsigned _488ns = 0;
                unsigned _61ns = 0;
                timestamp = (((((((_8ms << 3) | _1ms) << 8) | _4us) << 3) | _488ns) << 8) | _61ns;
                */
                *enableTist = true;
            }
            break;
        case 'y':
            if (service == ensemble->services.end()) {
                etiLog.printHeader(TcpLog::ERR, "You must define a service"
                        " before using option -%c\n", c);
                printUsage(progName);
                goto EXIT;
            }
            (*service)->pty = strtoul(optarg, NULL, 0);
            break;
        case 'z':
            *factumAnalyzer = true;
            break;
        case 'r':
            etiLog.printHeader(TcpLog::INFO,
                    "Enabling throttled output using simul, one frame every 24ms\n");
            outputs.push_back(new dabOutput);
            output = outputs.end() - 1;

            memset(*output, 0, sizeof(dabOutput));
            (*output)->outputProto = "simul";
            (*output)->outputName = "";
            (*output)->data = NULL;
            (*output)->operations = dabOutputDefaultOperations;

            subchannel = ensemble->subchannels.end();
            protection = NULL;
            component = ensemble->components.end();
            service = ensemble->services.end();
            break;
        case '?':
        case 'h':
            printUsage(progName, stdout);
            goto EXIT;
        default:
            etiLog.printHeader(TcpLog::ERR, "Option '%c' not coded yet\n", c);
            goto EXIT;
        }
    }
    if (optind < argc) {
        etiLog.printHeader(TcpLog::ERR, "Too much parameters:");
        while (optind < argc) {
            etiLog.printHeader(TcpLog::ERR, " %s", argv[optind++]);
        }
        etiLog.printHeader(TcpLog::ERR, "\n");
        printUsage(progName);
        goto EXIT;
    }

    return true;
EXIT:
    return false;
    
}
