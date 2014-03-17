/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2014
   Matthias P. Braendli, matthias.braendli@mpb.li
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

#include <vector>
#include <set>
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
#include "crc.h"
#include "UdpSocket.h"
#include "InetAddress.h"
#include "dabUtils.h"
#include "PcDebug.h"
#include "DabMux.h"
#include "MuxElements.h"
#include "utils.h"
#include "ParserCmdline.h"
#include "ParserConfigfile.h"
#include "StatsServer.h"
#include "Log.h"
#include "RemoteControl.h"

using namespace std;

/* Global stats server */
StatsServer* global_stats;

static unsigned char Padding_FIB[] = {
    0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/******************************************************************************
 *************  Tables pour l'identification des fichiers mp2  ****************
 *****************************************************************************/
const unsigned short Bit_Rate_SpecifiedTable[16] = {
    0, 32, 48, 56, 64, 80, 96, 112,
    128, 160, 192, 224, 256, 320, 384, 0
};



const unsigned char ProtectionLevelTable[64] = {
    4, 3, 2, 1, 0, 4, 3, 2,
    1, 0, 4, 3, 2, 1, 4, 3,
    2, 1, 0, 4, 3, 2, 1, 0,
    4, 3, 2, 1, 0, 4, 3, 2,
    1, 4, 3, 2, 1, 0, 4, 3,
    2, 1, 0, 4, 3, 2, 1, 0,
    4, 3, 2, 1, 0, 4, 3, 2,
    1, 0, 4, 3, 1, 4, 2, 0
};


const unsigned short BitRateTable[64] = {
    32, 32, 32, 32, 32, 48, 48, 48,
    48, 48, 56, 56, 56, 56, 64, 64,
    64, 64, 64, 80, 80, 80, 80, 80,
    96, 96, 96, 96, 96, 112, 112, 112,
    112, 128, 128, 128, 128, 128, 160, 160,
    160, 160, 160, 192, 192, 192, 192, 192,
    224, 224, 224, 224, 224, 256, 256, 256,
    256, 256, 320, 320, 320, 384, 384, 384
};


bool running = true;

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
    running = false;
}


int main(int argc, char *argv[])
{

    header_message();

    /*    for (int signum = 1; signum < 16; ++signum) {
          signal(signum, signalHandler);
          }*/
#ifndef _WIN32
    signal(SIGHUP, signalHandler);
    signal(SIGQUIT, signalHandler);
#endif
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    //signal(SIGPIPE, signalHandler);

#ifdef _WIN32
    if (SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST) == 0) {
        etiLog.log(warn, "Can't increase priority: %s\n",
                strerror(errno));
    }
#else
    if (setpriority(PRIO_PROCESS, 0, -20) == -1) {
        etiLog.log(warn, "Can't increase priority: %s\n",
                strerror(errno));
    }
#endif
    /*sched_param scheduler;
      scheduler.sched_priority = 99;        // sched_get_priority_max(SCHED_RR)
      if (sched_setscheduler(0, SCHED_RR, &scheduler) == -1) {
      etiLog.log(warn, "Can't increased priority: %s\n",
      strerror(errno));
      }*/

    uint8_t watermarkData[128];
    size_t watermarkSize = 0;
    size_t watermarkPos = 0;
    {
        uint8_t buffer[sizeof(watermarkData) / 2];
        snprintf((char*)buffer, sizeof(buffer),
                "%s %s, compiled at %s, %s",
                PACKAGE_NAME, PACKAGE_VERSION, __DATE__, __TIME__);
        memset(watermarkData, 0, sizeof(watermarkData));
        watermarkData[0] = 0x55; // Sync
        watermarkData[1] = 0x55;
        watermarkSize = 16;
        for (unsigned i = 0; i < strlen((char*)buffer); ++i) {
            for (int j = 0; j < 8; ++j) {
                uint8_t bit = (buffer[watermarkPos >> 3] >> (7 - (watermarkPos & 0x07))) & 1;
                watermarkData[watermarkSize >> 3] |= bit << (7 - (watermarkSize & 0x07));
                ++watermarkSize;
                bit = 1;
                watermarkData[watermarkSize >> 3] |= bit << (7 - (watermarkSize & 0x07));
                ++watermarkSize;
                ++watermarkPos;
            }
        }
    }
    watermarkPos = 0;


    dabEnsemble* ensemble = new dabEnsemble;
    ensemble->label.setLabel(DEFAULT_ENSEMBLE_LABEL, DEFAULT_ENSEMBLE_SHORT_LABEL);
    ensemble->mode = DEFAULT_DAB_MODE;
    ensemble->id = DEFAULT_ENSEMBLE_ID;
    ensemble->ecc = DEFAULT_ENSEMBLE_ECC;

    vector<dabOutput*> outputs;
    vector<DabService*>::iterator service = ensemble->services.end();
    vector<DabService*>::iterator serviceProgramInd;
    vector<DabService*>::iterator serviceDataInd;
    vector<DabService*>::iterator servicePty;
    vector<DabComponent*>::iterator component = ensemble->components.end();
    vector<DabComponent*>::iterator componentIndicatorProgram;
    vector<DabComponent*>::iterator componentIndicatorData;
    vector<dabSubchannel*>::iterator subchannel = ensemble->subchannels.end();
    vector<dabSubchannel*>::iterator subchannelIndicator;
    vector<dabOutput*>::iterator output;
    dabProtection* protection = NULL;

    BaseRemoteController* rc;

    unsigned int currentFrame;
    int returnCode = 0;
    int result;
    int cur;
    unsigned char etiFrame[6144];
    unsigned short index = 0;
    //FIC Length, DAB Mode I, II, IV -> FICL = 24, DAB Mode III -> FICL = 32
    unsigned FICL  = (ensemble->mode == 3 ? 32 : 24);

    unsigned long sync = 0x49C5F8;
    unsigned short FLtmp = 0;
    unsigned short nbBytesCRC = 0;
    unsigned short CRCtmp = 0xFFFF;
    unsigned short MSTsize = 0;

    unsigned int insertFIG = 0;
    unsigned int alterneFIB = 0;

    bool factumAnalyzer = false;
    unsigned long limit = 0;
    time_t date;
    bool enableTist = false;
    unsigned timestamp = 0;

    int statsserverport = 0;

    unsigned long time_seconds = 0;

    struct timeval mnsc_time;

    /* TODO: 
     * In a SFN, when reconfiguring the ensemble, the multiplexer
     * has to be restarted (odr-dabmux doesn't support reconfiguration).
     * Ideally, we must be able to restart transmission s.t. the receiver
     * synchronisation is preserved.
     */
    gettimeofday(&mnsc_time, NULL);

    bool MNSC_increment_time = false;

    if (argc > 1 && strncmp(argv[1], "-e", 2) == 0) { // use external config file
        try {

            if (argc != 3) {
                printUsage(argv[0], stderr);
                goto EXIT;
            }

            string conf_file = argv[2];

            parse_configfile(conf_file, outputs, ensemble, &enableTist, &FICL,
                    &factumAnalyzer, &limit, &rc, &statsserverport);

        }
        catch (runtime_error &e) {
            etiLog.log(error, "Configuration file parsing error: %s\n",
                    e.what());
            goto EXIT;
        }
    }
    else {
        if (!parse_cmdline(argv, argc, outputs, ensemble, &enableTist, &FICL,
                    &factumAnalyzer, &limit)) {
            goto EXIT;
        }
    }

    if (statsserverport != 0) {
        global_stats = new StatsServer(statsserverport);
    }
    else {
        global_stats = new StatsServer();
    }


    etiLog.level(info) <<
            PACKAGE_NAME << " " <<
#if defined(GITVERSION)
            GITVERSION <<
#else
            PACKAGE_VERSION <<
#endif
            " starting up";

    if (outputs.size() == 0) {
        etiLog.log(emerg, "no output defined");
        goto EXIT;
    }

    // Check and adjust subchannels
    {
        set<unsigned char> ids;
        for (subchannel = ensemble->subchannels.begin();
                subchannel != ensemble->subchannels.end();
                ++subchannel) {
            if (ids.find((*subchannel)->id) != ids.end()) {
                etiLog.log(error,
                        "Subchannel %u is set more than once!\n",
                        (*subchannel)->id);
                returnCode = -1;
                goto EXIT;
            }
        }
    }

    // Check and adjust services and components
    {
        set<uint32_t> ids;
        for (service = ensemble->services.begin();
                service != ensemble->services.end();
                ++service) {
            if (ids.find((*service)->id) != ids.end()) {
                etiLog.log(error,
                        "Service id 0x%x (%u) is set more than once!\n",
                        (*service)->id, (*service)->id);
                returnCode = -1;
                goto EXIT;
            }

            // Get first component of this service
            component = getComponent(ensemble->components, (*service)->id);
            if (component == ensemble->components.end()) {
                etiLog.log(error,
                        "Service id 0x%x (%u) includes no component!\n",
                        (*service)->id, (*service)->id);
                returnCode = -1;
                goto EXIT;
            }

            // Adjust service type from this first component
            switch ((*service)->getType(ensemble)) {
            case 0: // Audio
                (*service)->program = true;
                break;
            case 1:
            case 2:
            case 3:
                (*service)->program = false;
                break;
            default:
                etiLog.log(error,
                        "Error, unknown service type: %u\n", (*service)->getType(ensemble));
                returnCode = -1;
                goto EXIT;
            }

            // Adjust components type for DAB+
            while (component != ensemble->components.end()) {
                subchannel =
                    getSubchannel(ensemble->subchannels, (*component)->subchId);
                if (subchannel == ensemble->subchannels.end()) {
                    etiLog.log(error, "Error, service %u component "
                            "links to the invalid subchannel %u\n",
                            (*component)->serviceId, (*component)->subchId);
                    returnCode = -1;
                    goto EXIT;
                }

                protection = &(*subchannel)->protection;
                switch ((*subchannel)->type) {
                case 0: // Audio
                    {
                        if (protection->form != 0) {
                            (*component)->type = 0x3f;  // DAB+
                        }
                    }
                    break;
                case 1:
                case 2:
                case 3:
                    break;
                default:
                    etiLog.log(error,
                            "Error, unknown subchannel type\n");
                    returnCode = -1;
                    goto EXIT;
                }
                component = getComponent(ensemble->components,
                        (*service)->id, component);
            }
        }
    }


    for (output = outputs.begin(); output != outputs.end() ; ++output) {
#if defined(HAVE_OUTPUT_FILE)
        if ((*output)->outputProto == "file") {
            (*output)->output = new DabOutputFile();
#else // !defined(HAVE_OUTPUT_FILE)
        if (0) {
#endif // defined(HAVE_OUTPUT_FILE)
#if defined(HAVE_OUTPUT_FIFO)
        } else if ((*output)->outputProto == "fifo") {
            (*output)->output = new DabOutputFifo();
#endif // !defined(HAVE_OUTPUT_FIFO)
#if defined(HAVE_OUTPUT_RAW)
        } else if ((*output)->outputProto == "raw") {
            (*output)->output = new DabOutputRaw();
#endif // defined(HAVE_OUTPUT_RAW)
#if defined(HAVE_OUTPUT_UDP)
        } else if ((*output)->outputProto == "udp") {
            (*output)->output = new DabOutputUdp();
#endif // defined(HAVE_OUTPUT_UDP)
#if defined(HAVE_OUTPUT_TCP)
        } else if ((*output)->outputProto == "tcp") {
            (*output)->output = new DabOutputTcp();
#endif // defined(HAVE_OUTPUT_TCP)
#if defined(HAVE_OUTPUT_SIMUL)
        } else if ((*output)->outputProto == "simul") {
            (*output)->output = new DabOutputSimul();
#endif // defined(HAVE_OUTPUT_SIMUL)
#if defined(HAVE_OUTPUT_ZEROMQ)
        } else if ((*output)->outputProto == "zmq+tcp") {
            (*output)->output = new DabOutputZMQ("tcp");
        } else if ((*output)->outputProto == "zmq+ipc") {
            (*output)->output = new DabOutputZMQ("ipc");
        } else if ((*output)->outputProto == "zmq+pgm") {
            (*output)->output = new DabOutputZMQ("pgm");
        } else if ((*output)->outputProto == "zmq+epgm") {
            (*output)->output = new DabOutputZMQ("epgm");
#endif // defined(HAVE_OUTPUT_ZEROMQ)
        } else {
            etiLog.log(error, "Output protcol unknown: %s\n",
                    (*output)->outputProto.c_str());
            goto EXIT;
        }

        if ((*output)->output == NULL) {
            etiLog.log(error, "Unable to init output %s://%s\n",
                    (*output)->outputProto.c_str(), (*output)->outputName.c_str());
            return -1;
        }
        if ((*output)->output->Open((*output)->outputName)
                == -1) {
            etiLog.log(error, "Unable to open output %s://%s\n",
                    (*output)->outputProto.c_str(), (*output)->outputName.c_str());
            return -1;
        }
    }

    // Prepare and check the data inputs
    for (subchannel = ensemble->subchannels.begin();
            subchannel != ensemble->subchannels.end();
            ++subchannel) {
        protection = &(*subchannel)->protection;
        if (subchannel == ensemble->subchannels.begin()) {
            (*subchannel)->startAddress = 0;
        } else {
            (*subchannel)->startAddress = (*(subchannel - 1))->startAddress +
                getSizeCu(*(subchannel - 1));
        }
        if ((*subchannel)->input->open((*subchannel)->inputName) == -1) {
            perror((*subchannel)->inputName);
            returnCode = -1;
            goto EXIT;
        }

        // TODO Check errors
        result = (*subchannel)->input->setBitrate( (*subchannel)->bitrate);
        if (result <= 0) {
            etiLog.log(error, "can't set bitrate for source %s\n",
                    (*subchannel)->inputName);
            returnCode = -1;
            goto EXIT;
        }
        (*subchannel)->bitrate = result;

        if (protection->form == 0) {
            protection->form = 1;
            for (int i = 0; i < 64; i++) {
                if ((*subchannel)->bitrate == BitRateTable[i]) {
                    if (protection->level == ProtectionLevelTable[i]) {
                        protection->form = 0;
                        protection->shortForm.tableIndex = i;
                    }
                }
            }
        }
    }

    if (ensemble->subchannels.size() == 0) {
        etiLog.log(error, "can't multiplexed no subchannel!\n");
        returnCode = -1;
        goto EXIT;
    }

    subchannel = ensemble->subchannels.end() - 1;
    if ((*subchannel)->startAddress + getSizeCu((*subchannel)) > 864) {
        etiLog.log(error, "Total size in CU exceeds 864\n");
        printSubchannels(ensemble->subchannels);
        returnCode = -1;
        goto EXIT;
    }

    // Init packet components SCId
    cur = 0;
    for (component = ensemble->components.begin();
            component != ensemble->components.end();
            ++component) {
        subchannel = getSubchannel(ensemble->subchannels,
                (*component)->subchId);
        if (subchannel == ensemble->subchannels.end()) {
            etiLog.log(error,
                    "Subchannel %i does not exist for component "
                    "of service %i\n",
                    (*component)->subchId, (*component)->serviceId);
            returnCode = -1;
            goto EXIT;
        }
        if ((*subchannel)->type != 3) continue;

        (*component)->packet.id = cur++;
    }

    // Print settings before starting
    etiLog.log(info, "--- Multiplex configuration ---");
    printEnsemble(ensemble);

    etiLog.log(info, "--- Subchannels list ---");
    printSubchannels(ensemble->subchannels);

    etiLog.log(info, "--- Services list ---");
    printServices(ensemble->services);

    etiLog.log(info, "--- Components list ---");
    printComponents(ensemble->components);

    etiLog.log(info, "--- Output list ---");
    printOutputs(outputs);



    /*   Each iteration of the main loop creates one ETI frame */

    serviceProgramInd = ensemble->services.end();
    serviceDataInd = ensemble->services.end();
    componentIndicatorProgram = ensemble->components.end();
    componentIndicatorData = ensemble->components.end();
    servicePty = ensemble->services.end();
    subchannelIndicator = ensemble->subchannels.end();


    for (currentFrame = 0; running; currentFrame++) {
        if ((limit > 0) && (currentFrame >= limit)) {
            break;
        }
        date = getDabTime();

        // Initialise the ETI frame
        memset(etiFrame, 0, 6144);

        /**********************************************************************
         **********   Section SYNC of ETI(NI, G703)   *************************
         **********************************************************************/

        // See ETS 300 799 Clause 6
        eti_SYNC *etiSync = (eti_SYNC *) etiFrame;

        etiSync->ERR = 0xFF; // ETS 300 799, 5.2, no error

        //****** Field FSYNC *****//
        // See ETS 300 799, 6.2.1.2
        sync ^= 0xffffff;
        etiSync->FSYNC = sync;

        /**********************************************************************
         ***********   Section LIDATA of ETI(NI, G703)   **********************
         **********************************************************************/

        // See ETS 300 799 Figure 5 for a better overview of these fields.

        //****** Section FC ***************************************************/
        // 4 octets, starts at offset 4
        eti_FC *fc = (eti_FC *) &etiFrame[4];

        //****** FCT ******//
        // Incremente for each frame, overflows at 249
        fc->FCT = currentFrame % 250;

        //****** FICF ******//
        // Fast Information Channel Flag, 1 bit, =1 if FIC present
        fc->FICF = 1;

        //****** NST ******//
        /* Number of audio of data sub-channels, 7 bits, 0-64.
         * In the 15-frame period immediately preceding a multiplex
         * re-configuration, NST can take the value 0 (see annex E).
         */
        fc->NST = ensemble->subchannels.size();

        //****** FP ******//
        /* Frame Phase, 3 bit counter, tells the COFDM generator
         * when to insert the TII. Is also used by the MNSC.
         */
        fc->FP = currentFrame & 0x7;

        //****** MID ******//
        //Mode Identity, 2 bits, 01 ModeI, 10 modeII, 11 ModeIII, 00 ModeIV
        fc->MID = ensemble->mode;      //mode 2 needs 3 FIB, 3*32octets = 96octets

        //****** FL ******//
        /* Frame Length, 11 bits, nb of words(4 bytes) in STC, EOH and MST
         * if NST=0, FL=1+FICL words, FICL=24 or 32 depending on the mode.
         * The FL is given in words (4 octets), see ETS 300 799 5.3.6 for details
         */
        FLtmp = 1 + FICL + (fc->NST);

        for (subchannel = ensemble->subchannels.begin();
                subchannel != ensemble->subchannels.end();
                ++subchannel) {
            // Add STLsbch
            FLtmp += getSizeWord(*subchannel);
        }

        fc->setFrameLength(FLtmp);
        index = 8;

        /******* Section STC **************************************************/
        // Stream Characterization,
        //  number of channel * 4 octets = nb octets total
        for (subchannel = ensemble->subchannels.begin();
                subchannel != ensemble->subchannels.end();
                ++subchannel) {
            protection = &(*subchannel)->protection;
            eti_STC *sstc = (eti_STC *) & etiFrame[index];

            sstc->SCID = (*subchannel)->id;
            sstc->startAddress_high = (*subchannel)->startAddress / 256;
            sstc->startAddress_low = (*subchannel)->startAddress % 256;
            //devrait changer selon le mode de protection desire
            if (protection->form == 0) {
                sstc->TPL = 0x10 |
                    ProtectionLevelTable[protection->shortForm.tableIndex];
            } else {
                sstc->TPL = 0x20 |
                    (protection->longForm.option << 2) |
                    (protection->level);
            }
            //Sub-channel Stream Length, multiple de 64 bits 
            sstc->STL_high = getSizeDWord(*subchannel) / 256;
            sstc->STL_low = getSizeDWord(*subchannel) % 256;
            index += 4;
        }

        /******* Section EOH **************************************************/
        // End of Header 4 octets
        eti_EOH *eoh = (eti_EOH *) & etiFrame[index];

        //MNSC Multiplex Network Signalling Channel, 2 octets

        eoh->MNSC = 0;

        struct tm *time_tm = gmtime(&mnsc_time.tv_sec);
        switch (fc->FP & 0x3)
        {
            case 0:
                if (MNSC_increment_time)
                {
                    MNSC_increment_time = false;
                    mnsc_time.tv_sec += 1;
                }
                {

                    eti_MNSC_TIME_0 *mnsc = (eti_MNSC_TIME_0 *) &eoh->MNSC;
                    // Set fields according to ETS 300 799 -- 5.5.1 and A.2.2 
                    mnsc->type = 0;
                    mnsc->identifier = 0;
                    mnsc->rfa = 0;
                }
                break;
            case 1:
                {
                    eti_MNSC_TIME_1 *mnsc = (eti_MNSC_TIME_1 *) &eoh->MNSC;
                    mnsc->setFromTime(time_tm);
                    mnsc->accuracy = 1;
                    mnsc->sync_to_frame = 1;
                }
                break;
            case 2:
                {
                    eti_MNSC_TIME_2 *mnsc = (eti_MNSC_TIME_2 *) &eoh->MNSC;
                    mnsc->setFromTime(time_tm);
                }
                break;
            case 3:
                {
                    eti_MNSC_TIME_3 *mnsc = (eti_MNSC_TIME_3 *) &eoh->MNSC;
                    mnsc->setFromTime(time_tm);
                }
                break;
        }


        //CRC Cyclic Redundancy Checksum du FC,  STC et MNSC, 2 octets
        nbBytesCRC = 4 + ((fc->NST) * 4) + 2;

        CRCtmp = 0xffff;
        CRCtmp = crc16(CRCtmp, &etiFrame[4], nbBytesCRC);
        CRCtmp ^= 0xffff;
        eoh->CRC = htons(CRCtmp);

        /******* Section MST **************************************************/
        // Main Stream Data, si FICF=1 alors les 96 ou 128 premiers octets
        // transportent le FIC selon le mode
        index = ((fc->NST) + 2 + 1) * 4;

        //Insertion du FIC
        FIGtype0* fig0;
        FIGtype0_0 *fig0_0;
        FIGtype0_1 *figtype0_1;

        FIG_01_SubChannel_ShortF *fig0_1subchShort;
        FIG_01_SubChannel_LongF *fig0_1subchLong1;

        FIGtype0_2 *fig0_2;

        FIGtype0_2_Service *fig0_2serviceAudio;
        FIGtype0_2_Service_data *fig0_2serviceData;
        FIGtype0_2_audio_component* audio_description;
        FIGtype0_2_data_component* data_description;
        FIGtype0_2_packet_component* packet_description;

        FIGtype0_3_header *fig0_3_header;
        FIGtype0_3_data *fig0_3_data;
        FIGtype0_9 *fig0_9;
        FIGtype0_10 *fig0_10;
        FIGtype0_17_programme *programme;

        FIGtype1_0 *fig1_0;
        FIGtype1_1 *fig1_1;
        FIGtype1_5 *fig1_5;

        tm* timeData;

        unsigned char figSize = 0;

        //Insertion du FIB 0
        switch (insertFIG) {

        case 0:
        case 4:
            // FIG type 0/0, Multiplex Configuration Info (MCI),
            //  Ensemble information
            fig0_0 = (FIGtype0_0 *) & etiFrame[index];

            fig0_0->FIGtypeNumber = 0;
            fig0_0->Length = 5;
            fig0_0->CN = 0;
            fig0_0->OE = 0;
            fig0_0->PD = 0;
            fig0_0->Extension = 0;

            fig0_0->EId = htons(ensemble->id);
            fig0_0->Change = 0;
            fig0_0->Al = 0;
            fig0_0->CIFcnt_hight = (currentFrame / 250) % 20;
            fig0_0->CIFcnt_low = (currentFrame % 250);
            index = index + 6;
            figSize += 6;

            break;

        case 1:
            // FIG type 0/1, MIC, Sub-Channel Organization, une instance de la
            // sous-partie par sous-canal
            figtype0_1 = (FIGtype0_1 *) & etiFrame[index];

            figtype0_1->FIGtypeNumber = 0;
            figtype0_1->Length = 1;
            figtype0_1->CN = 0;
            figtype0_1->OE = 0;
            figtype0_1->PD = 0;
            figtype0_1->Extension = 1;
            index = index + 2;
            figSize += 2;

            //Sous-partie du FIG type 0/1
            if (subchannelIndicator == ensemble->subchannels.end()) {
                subchannelIndicator = ensemble->subchannels.begin();
            }
            for (; subchannelIndicator != ensemble->subchannels.end();
                    ++subchannelIndicator) {
                protection = &(*subchannelIndicator)->protection;

                if ((protection->form == 0 && figSize > 27) ||
                        (protection->form != 0 && figSize > 26)) {
                    break;
                }

                if (protection->form == 0) {
                    fig0_1subchShort =
                        (FIG_01_SubChannel_ShortF*) &etiFrame[index];
                    fig0_1subchShort->SubChId = (*subchannelIndicator)->id;
                    fig0_1subchShort->StartAdress_high =
                        (*subchannelIndicator)->startAddress / 256;
                    fig0_1subchShort->StartAdress_low =
                        (*subchannelIndicator)->startAddress % 256;
                    fig0_1subchShort->Short_Long_form = 0;
                    fig0_1subchShort->TableSwitch = 0;
                    fig0_1subchShort->TableIndex =
                        protection->shortForm.tableIndex;

                    index = index + 3;
                    figSize += 3;
                    figtype0_1->Length += 3;
                } else {
                    fig0_1subchLong1 =
                        (FIG_01_SubChannel_LongF*) &etiFrame[index];
                    fig0_1subchLong1->SubChId = (*subchannelIndicator)->id;
                    fig0_1subchLong1->StartAdress_high =
                        (*subchannelIndicator)->startAddress / 256;
                    fig0_1subchLong1->StartAdress_low =
                        (*subchannelIndicator)->startAddress % 256;
                    fig0_1subchLong1->Short_Long_form = 1;
                    fig0_1subchLong1->Option = protection->longForm.option;
                    fig0_1subchLong1->ProtectionLevel =
                        protection->level;
                    fig0_1subchLong1->Sub_ChannelSize_high =
                        getSizeCu(*subchannelIndicator) / 256;
                    fig0_1subchLong1->Sub_ChannelSize_low =
                        getSizeCu(*subchannelIndicator) % 256;

                    index = index + 4;
                    figSize += 4;
                    figtype0_1->Length += 4;
                }
            }
            break;

        case 2:
            // FIG type 0/2, MCI, Service Organization, une instance de
            // FIGtype0_2_Service par sous-canal
            fig0_2 = NULL;
            cur = 0;

            if (serviceProgramInd == ensemble->services.end()) {
                serviceProgramInd = ensemble->services.begin();
            }
            for (; serviceProgramInd != ensemble->services.end();
                    ++serviceProgramInd) {
                if (!(*serviceProgramInd)->nbComponent(ensemble->components)) {
                    continue;
                }
                if ((*serviceProgramInd)->getType(ensemble) != 0) {
                    continue;
                }

                ++cur;

                if (fig0_2 == NULL) {
                    fig0_2 = (FIGtype0_2 *) & etiFrame[index];

                    fig0_2->FIGtypeNumber = 0;
                    fig0_2->Length = 1;
                    fig0_2->CN = 0;
                    fig0_2->OE = 0;
                    fig0_2->PD = 0;
                    fig0_2->Extension = 2;
                    index = index + 2;
                    figSize += 2;
                }

                if (figSize + 3
                        + (*serviceProgramInd)->nbComponent(ensemble->components)
                        * 2 > 30) {
                    break;
                }

                fig0_2serviceAudio = (FIGtype0_2_Service*) &etiFrame[index];

                fig0_2serviceAudio->SId = htons((*serviceProgramInd)->id);
                fig0_2serviceAudio->Local_flag = 0;
                fig0_2serviceAudio->CAId = 0;
                fig0_2serviceAudio->NbServiceComp =
                    (*serviceProgramInd)->nbComponent(ensemble->components);
                index += 3;
                fig0_2->Length += 3;
                figSize += 3;

                int curCpnt = 0;
                for (component = getComponent(ensemble->components,
                            (*serviceProgramInd)->id);
                        component != ensemble->components.end();
                        component = getComponent(ensemble->components,
                            (*serviceProgramInd)->id, component)) {
                    subchannel = getSubchannel(ensemble->subchannels,
                            (*component)->subchId);
                    if (subchannel == ensemble->subchannels.end()) {
                        etiLog.log(error,
                                "Subchannel %i does not exist for component "
                                "of service %i\n",
                                (*component)->subchId, (*component)->serviceId);
                        returnCode = -1;
                        goto EXIT;
                    }

                    switch ((*subchannel)->type) {
                    case 0: // Audio
                        audio_description =
                            (FIGtype0_2_audio_component*)&etiFrame[index];
                        audio_description->TMid = 0;
                        audio_description->ASCTy = (*component)->type;
                        audio_description->SubChId = (*subchannel)->id;
                        audio_description->PS = ((curCpnt == 0) ? 1 : 0);
                        audio_description->CA_flag = 0;
                        break;
                    case 1: // Data
                        data_description =
                            (FIGtype0_2_data_component*)&etiFrame[index];
                        data_description->TMid = 1;
                        data_description->DSCTy = (*component)->type;
                        data_description->SubChId = (*subchannel)->id;
                        data_description->PS = ((curCpnt == 0) ? 1 : 0);
                        data_description->CA_flag = 0;
                        break;
                    case 3: // Packet
                        packet_description =
                            (FIGtype0_2_packet_component*)&etiFrame[index];
                        packet_description->TMid = 3;
                        packet_description->setSCId((*component)->packet.id);
                        packet_description->PS = ((curCpnt == 0) ? 1 : 0);
                        packet_description->CA_flag = 0;
                        break;
                    default:
                        etiLog.log(error,
                                "Component type not supported\n");
                        returnCode = -1;
                        goto EXIT;
                    }
                    index += 2;
                    fig0_2->Length += 2;
                    figSize += 2;
                    if (figSize > 30) {
                        etiLog.log(error,
                                "Sorry, no place left in FIG 0/2 to insert "
                                "component %i of program service %i.\n",
                                curCpnt, cur);
                        returnCode = -1;
                        goto EXIT;
                    }
                    ++curCpnt;
                }
            }
            break;

        case 3:
            fig0_2 = NULL;
            cur = 0;

            if (serviceDataInd == ensemble->services.end()) {
                serviceDataInd = ensemble->services.begin();
            }
            for (; serviceDataInd != ensemble->services.end();
                    ++serviceDataInd) {
                if (!(*serviceDataInd)->nbComponent(ensemble->components)) {
                    continue;
                }
                unsigned char type = (*serviceDataInd)->getType(ensemble);
                if ((type == 0) || (type == 2)) {
                    continue;
                }

                ++cur;

                if (fig0_2 == NULL) {
                    fig0_2 = (FIGtype0_2 *) & etiFrame[index];

                    fig0_2->FIGtypeNumber = 0;
                    fig0_2->Length = 1;
                    fig0_2->CN = 0;
                    fig0_2->OE = 0;
                    fig0_2->PD = 1;
                    fig0_2->Extension = 2;
                    index = index + 2;
                    figSize += 2;
                }

                if (figSize + 5
                        + (*serviceDataInd)->nbComponent(ensemble->components)
                        * 2 > 30) {
                    break;
                }

                fig0_2serviceData =
                    (FIGtype0_2_Service_data*) &etiFrame[index];

                fig0_2serviceData->SId = htonl((*serviceDataInd)->id);
                fig0_2serviceData->Local_flag = 0;
                fig0_2serviceData->CAId = 0;
                fig0_2serviceData->NbServiceComp =
                    (*serviceDataInd)->nbComponent(ensemble->components);
                fig0_2->Length += 5;
                index += 5;
                figSize += 5;

                int curCpnt = 0;
                for (component = getComponent(ensemble->components,
                            (*serviceDataInd)->id);
                        component != ensemble->components.end();
                        component = getComponent(ensemble->components,
                            (*serviceDataInd)->id, component)) {
                    subchannel = getSubchannel(ensemble->subchannels,
                            (*component)->subchId);
                    if (subchannel == ensemble->subchannels.end()) {
                        etiLog.log(error,
                                "Subchannel %i does not exist for component "
                                "of service %i\n",
                                (*component)->subchId, (*component)->serviceId);
                        returnCode = -1;
                        goto EXIT;
                    }

                    switch ((*subchannel)->type) {
                    case 0: // Audio
                        audio_description =
                            (FIGtype0_2_audio_component*)&etiFrame[index];
                        audio_description->TMid = 0;
                        audio_description->ASCTy = (*component)->type;
                        audio_description->SubChId = (*subchannel)->id;
                        audio_description->PS = ((curCpnt == 0) ? 1 : 0);
                        audio_description->CA_flag = 0;
                        break;
                    case 1: // Data
                        data_description =
                            (FIGtype0_2_data_component*)&etiFrame[index];
                        data_description->TMid = 1;
                        data_description->DSCTy = (*component)->type;
                        data_description->SubChId = (*subchannel)->id;
                        data_description->PS = ((curCpnt == 0) ? 1 : 0);
                        data_description->CA_flag = 0;
                        break;
                    case 3: // Packet
                        packet_description =
                            (FIGtype0_2_packet_component*)&etiFrame[index];
                        packet_description->TMid = 3;
                        packet_description->setSCId((*component)->packet.id);
                        packet_description->PS = ((curCpnt == 0) ? 1 : 0);
                        packet_description->CA_flag = 0;
                        break;
                    default:
                        etiLog.log(error,
                                "Component type not supported\n");
                        returnCode = -1;
                        goto EXIT;
                    }
                    index += 2;
                    fig0_2->Length += 2;
                    figSize += 2;
                    if (figSize > 30) {
                        etiLog.log(error,
                                "Sorry, no place left in FIG 0/2 to insert "
                                "component %i of data service %i.\n",
                                curCpnt, cur);
                        returnCode = -1;
                        goto EXIT;
                    }
                    ++curCpnt;
                }
            }
            break;

        case 5:
            fig0_3_header = NULL;

            for (component = ensemble->components.begin();
                    component != ensemble->components.end();
                    ++component) {
                subchannel = getSubchannel(ensemble->subchannels,
                        (*component)->subchId);
                if (subchannel == ensemble->subchannels.end()) {
                    etiLog.log(error,
                            "Subchannel %i does not exist for component "
                            "of service %i\n",
                            (*component)->subchId, (*component)->serviceId);
                    returnCode = -1;
                    goto EXIT;
                }

                if ((*subchannel)->type != 3) continue;

                if (fig0_3_header == NULL) {
                    fig0_3_header = (FIGtype0_3_header*)&etiFrame[index];
                    fig0_3_header->FIGtypeNumber = 0;
                    fig0_3_header->Length = 1;
                    fig0_3_header->CN = 0;
                    fig0_3_header->OE = 0;
                    fig0_3_header->PD = 0;
                    fig0_3_header->Extension = 3;

                    index += 2;
                    figSize += 2;
                }

                /*  Warning: When bit SCCA_flag is unset(0), the multiplexer
                 *  R&S does not send the SCCA field. But, in the Factum ETI
                 *  analyzer, if this field is not there, it is an error.
                 */
                fig0_3_data = (FIGtype0_3_data*)&etiFrame[index];
                fig0_3_data->setSCId((*component)->packet.id);
                fig0_3_data->rfa = 0;
                fig0_3_data->SCCA_flag = 0;
                // if 0, datagroups are used
                fig0_3_data->DG_flag = !(*component)->packet.datagroup;
                fig0_3_data->rfu = 0;
                fig0_3_data->DSCTy = (*component)->type;
                fig0_3_data->SubChId = (*subchannel)->id;
                fig0_3_data->setPacketAddress((*component)->packet.address);
                if (factumAnalyzer) {
                    fig0_3_data->SCCA = 0;
                }

                fig0_3_header->Length += 5;
                index += 5;
                figSize += 5;
                if (factumAnalyzer) {
                    fig0_3_header->Length += 2;
                    index += 2;
                    figSize += 2;
                }

                if (figSize > 30) {
                    etiLog.log(error,
                            "can't add to Fic Fig 0/3, "
                            "too much packet service\n");
                    returnCode = -1;
                    goto EXIT;
                }
            }
            break;

        case 7:
            fig0 = NULL;
            if (servicePty == ensemble->services.end()) {
                servicePty = ensemble->services.begin();
            }
            for (; servicePty != ensemble->services.end();
                    ++servicePty) {
                if ((*servicePty)->pty == 0 && (*servicePty)->language == 0) {
                    continue;
                }
                if (fig0 == NULL) {
                    fig0 = (FIGtype0*)&etiFrame[index];
                    fig0->FIGtypeNumber = 0;
                    fig0->Length = 1;
                    fig0->CN = 0;
                    fig0->OE = 0;
                    fig0->PD = 0;
                    fig0->Extension = 17;
                    index += 2;
                    figSize += 2;
                }

                if ((*servicePty)->language == 0) {
                    if (figSize + 4 > 30) {
                        break;
                    }
                } else {
                    if (figSize + 5 > 30) {
                        break;
                    }
                }

                programme = 
                    (FIGtype0_17_programme*)&etiFrame[index];
                programme->SId = htons((*servicePty)->id);
                programme->SD = 1;
                programme->PS = 0;
                programme->L = (*servicePty)->language != 0;
                programme->CC = 0;
                programme->Rfa = 0;
                programme->NFC = 0;
                if ((*servicePty)->language == 0) {
                    etiFrame[index + 3] = (*servicePty)->pty;
                    fig0->Length += 4;
                    index += 4;
                    figSize += 4;
                } else {
                    etiFrame[index + 3] = (*servicePty)->language;
                    etiFrame[index + 4] = (*servicePty)->pty;
                    fig0->Length += 5;
                    index += 5;
                    figSize += 5;
                }
            }
            break;
        }

        if (figSize > 30) {
            etiLog.log(error,
                    "FIG too big (%i > 30)\n", figSize);
            returnCode = -1;
            goto EXIT;
        }

        memcpy(&etiFrame[index], Padding_FIB, 30 - figSize);
        index += 30 - figSize;

        CRCtmp = 0xffff;
        CRCtmp = crc16(CRCtmp, &etiFrame[(index - 30)], 30);
        CRCtmp ^= 0xffff;
        etiFrame[index++] = ((char *) &CRCtmp)[1];
        etiFrame[index++] = ((char *) &CRCtmp)[0];

        figSize = 0;
        // Insertion du FIB 1        
        switch (alterneFIB) {
        case 0:     // FIG 0/8 program
            fig0 = NULL;

            if (componentIndicatorProgram == ensemble->components.end()) {
                componentIndicatorProgram = ensemble->components.begin();
            }
            for (; componentIndicatorProgram != ensemble->components.end();
                    ++componentIndicatorProgram) {
                service = getService(*componentIndicatorProgram,
                        ensemble->services);
                subchannel = getSubchannel(ensemble->subchannels,
                        (*componentIndicatorProgram)->subchId);
                if (subchannel == ensemble->subchannels.end()) {
                    etiLog.log(error,
                            "Subchannel %i does not exist for component "
                            "of service %i\n",
                            (*componentIndicatorProgram)->subchId,
                            (*componentIndicatorProgram)->serviceId);
                    returnCode = -1;
                    goto EXIT;
                }

                if (!(*service)->program) continue;

                if (fig0 == NULL) {
                    fig0 = (FIGtype0*)&etiFrame[index];
                    fig0->FIGtypeNumber = 0;
                    fig0->Length = 1;
                    fig0->CN = 0;
                    fig0->OE = 0;
                    fig0->PD = 0;
                    fig0->Extension = 8;
                    index += 2;
                    figSize += 2;
                }

                if ((*subchannel)->type == 3) { // Data packet
                    if (figSize > 30 - 5) {
                        break;
                    }
                    *((uint16_t*)&etiFrame[index]) =
                        htons((*componentIndicatorProgram)->serviceId);
                    fig0->Length += 2;
                    index += 2;
                    figSize += 2;

                    FIGtype0_8_long* definition =
                        (FIGtype0_8_long*)&etiFrame[index];
                    memset(definition, 0, 3);
                    definition->ext = 0;    // no rfa
                    definition->SCIdS = (*componentIndicatorProgram)->SCIdS;
                    definition->LS = 1;
                    definition->setSCId((*componentIndicatorProgram)->packet.id);
                    fig0->Length += 3;
                    index += 3;             // 8 minus rfa
                    figSize += 3;
                } else {    // Audio, data stream or FIDC
                    if (figSize > 30 - 4) {
                        break;
                    }
                    *((uint16_t*)&etiFrame[index]) =
                        htons((*componentIndicatorProgram)->serviceId);
                    fig0->Length += 2;
                    index += 2;
                    figSize += 2;

                    FIGtype0_8_short* definition =
                        (FIGtype0_8_short*)&etiFrame[index];
                    memset(definition, 0, 2);
                    definition->ext = 0;    // no rfa
                    definition->SCIdS = (*componentIndicatorProgram)->SCIdS;
                    definition->LS = 0;
                    definition->MscFic = 0;
                    definition->Id = (*componentIndicatorProgram)->subchId;
                    fig0->Length += 2;
                    index += 2;             // 4 minus rfa
                    figSize += 2;
                }
            }
            break;

        case 1:     // FIG 0/8 data
            fig0 = NULL;

            if (componentIndicatorData == ensemble->components.end()) {
                componentIndicatorData = ensemble->components.begin();
            }
            for (; componentIndicatorData != ensemble->components.end();
                    ++componentIndicatorData) {
                service = getService(*componentIndicatorData,
                        ensemble->services);
                subchannel = getSubchannel(ensemble->subchannels,
                        (*componentIndicatorData)->subchId);
                if (subchannel == ensemble->subchannels.end()) {
                    etiLog.log(error,
                            "Subchannel %i does not exist for component "
                            "of service %i\n",
                            (*componentIndicatorData)->subchId,
                            (*componentIndicatorData)->serviceId);
                    returnCode = -1;
                    goto EXIT;
                }

                if ((*service)->program) continue;

                if (fig0 == NULL) {
                    fig0 = (FIGtype0*)&etiFrame[index];
                    fig0->FIGtypeNumber = 0;
                    fig0->Length = 1;
                    fig0->CN = 0;
                    fig0->OE = 0;
                    fig0->PD = 1;
                    fig0->Extension = 8;
                    index += 2;
                    figSize += 2;
                }

                if ((*subchannel)->type == 3) { // Data packet
                    if (figSize > 30 - 7) {
                        break;
                    }
                    *((uint32_t*)&etiFrame[index]) =
                        htonl((*componentIndicatorData)->serviceId);
                    fig0->Length += 4;
                    index += 4;
                    figSize += 4;

                    FIGtype0_8_long* definition =
                        (FIGtype0_8_long*)&etiFrame[index];
                    memset(definition, 0, 3);
                    definition->ext = 0;    // no rfa
                    definition->SCIdS = (*componentIndicatorData)->SCIdS;
                    definition->LS = 1;
                    definition->setSCId((*componentIndicatorData)->packet.id);
                    fig0->Length += 3;
                    index += 3;             // 8 minus rfa
                    figSize += 3;
                } else {    // Audio, data stream or FIDC
                    if (figSize > 30 - 6) {
                        break;
                    }
                    *((uint32_t*)&etiFrame[index]) =
                        htonl((*componentIndicatorData)->serviceId);
                    fig0->Length += 4;
                    index += 4;
                    figSize += 4;

                    FIGtype0_8_short* definition =
                        (FIGtype0_8_short*)&etiFrame[index];
                    memset(definition, 0, 2);
                    definition->ext = 0;    // no rfa
                    definition->SCIdS = (*componentIndicatorData)->SCIdS;
                    definition->LS = 0;
                    definition->MscFic = 0;
                    definition->Id = (*componentIndicatorData)->subchId;
                    fig0->Length += 2;
                    index += 2;             // 4 minus rfa
                    figSize += 2;
                }
            }
            break;

        case 3:
            // FIG type 1/0, Service Information (SI), Ensemble Label
            fig1_0 = (FIGtype1_0 *) & etiFrame[index];

            fig1_0->Length = 21;
            fig1_0->FIGtypeNumber = 1;
            fig1_0->Extension = 0;
            fig1_0->OE = 0;
            fig1_0->Charset = 0;
            fig1_0->EId = htons(ensemble->id);
            index = index + 4;

            memcpy(&etiFrame[index], ensemble->label.text(), 16);
            index = index + 16;

            etiFrame[index++] = ensemble->label.flag() >> 8;
            etiFrame[index++] = ensemble->label.flag() & 0xFF;

            figSize += 22;
            break;
        case 5:
            // FIG 0 / 13
            fig0 = NULL;

            for (component = ensemble->components.begin();
                    component != ensemble->components.end();
                    ++component) {
                subchannel = getSubchannel(ensemble->subchannels,
                        (*component)->subchId);
                if (subchannel == ensemble->subchannels.end()) {
                    etiLog.log(error,
                            "Subchannel %i does not exist for component "
                            "of service %i\n",
                            (*component)->subchId, (*component)->serviceId);
                    returnCode = -1;
                    goto EXIT;
                }

                if ((*subchannel)->type == 0) { // audio
                    if (fig0 == NULL) {
                        fig0 = (FIGtype0*)&etiFrame[index];
                        fig0->FIGtypeNumber = 0;
                        fig0->Length = 1;
                        fig0->CN = 0;
                        fig0->OE = 0;
                        fig0->PD = 1;
                        fig0->Extension = 13;
                        index += 2;
                        figSize += 2;
                    }

                    FIG0_13_longAppInfo* info =
                        (FIG0_13_longAppInfo*)&etiFrame[index];
                    info->SId = htonl((*component)->serviceId);
                    info->SCIdS = (*component)->SCIdS;
                    info->No = 1;
                    index += 5;
                    figSize += 5;
                    fig0->Length += 5;

                    FIG0_13_app* app = (FIG0_13_app*)&etiFrame[index];
                    app->setType(FIG0_13_APPTYPE_SLIDESHOW);
                    app->length = 4;
                    app->xpad = 0x0cbc0000;
                    /* xpad meaning
                       CA        = 0
                       CAOrg     = 0
                       Rfu       = 0
                       AppTy(5)  = 12 (MOT, start of X-PAD data group)
                       DG        = 1 (MSC data groups not used)
                       Rfu       = 0
                       DSCTy(6)  = 60 (MOT)
                       CAOrg(16) = 0
                    */

                    index += 2 + app->length;
                    figSize += 2 + app->length;
                    fig0->Length += 2 + app->length;
                }
                else if ((*subchannel)->type == 3 && // packet
                    (*component)->packet.appType != 0xffff) {

                    if (fig0 == NULL) {
                        fig0 = (FIGtype0*)&etiFrame[index];
                        fig0->FIGtypeNumber = 0;
                        fig0->Length = 1;
                        fig0->CN = 0;
                        fig0->OE = 0;
                        fig0->PD = 1;
                        fig0->Extension = 13;
                        index += 2;
                        figSize += 2;
                    }

                    FIG0_13_longAppInfo* info =
                        (FIG0_13_longAppInfo*)&etiFrame[index];
                    info->SId = htonl((*component)->serviceId);
                    info->SCIdS = (*component)->SCIdS;
                    info->No = 1;
                    index += 5;
                    figSize += 5;
                    fig0->Length += 5;

                    FIG0_13_app* app = (FIG0_13_app*)&etiFrame[index];
                    app->setType((*component)->packet.appType);
                    app->length = 0;
                    index += 2;
                    figSize += 2;
                    fig0->Length += 2;

                    if (figSize > 30) {
                        etiLog.log(error,
                                "can't add to Fic Fig 0/13, "
                                "too much packet service\n");
                        returnCode = -1;
                        goto EXIT;
                    }
                }
            }
            break;
        case 7:
            //Time and country identifier
            fig0_10 = (FIGtype0_10 *) & etiFrame[index];

            fig0_10->FIGtypeNumber = 0;
            fig0_10->Length = 5;
            fig0_10->CN = 0;
            fig0_10->OE = 0;
            fig0_10->PD = 0;
            fig0_10->Extension = 10;
            index = index + 2;

            timeData = localtime(&date);

            fig0_10->RFU = 0;
            fig0_10->setMJD(gregorian2mjd(timeData->tm_year + 1900,
                        timeData->tm_mon + 1,
                        timeData->tm_mday));
            fig0_10->LSI = 0;
            fig0_10->ConfInd = (watermarkData[watermarkPos >> 3] >>
                    (7 - (watermarkPos & 0x07))) & 1;
            if (++watermarkPos == watermarkSize) {
                watermarkPos = 0;
            }
            fig0_10->UTC = 0;
            fig0_10->setHours(timeData->tm_hour);
            fig0_10->Minutes = timeData->tm_min;
            index = index + 4;
            figSize += 6;

            fig0_9 = (FIGtype0_9*)&etiFrame[index];
            fig0_9->FIGtypeNumber = 0;
            fig0_9->Length = 4;
            fig0_9->CN = 0;
            fig0_9->OE = 0;
            fig0_9->PD = 0;
            fig0_9->Extension = 9;

            fig0_9->ext = 0;
            fig0_9->lto = 0;
            fig0_9->ensembleLto = 0;
            fig0_9->ensembleEcc = ensemble->ecc;
            fig0_9->tableId = 0x2;
            index += 5;
            figSize += 5;

            break;
        }

        assert(figSize <= 30);
        memcpy(&etiFrame[index], Padding_FIB, 30 - figSize);
        index += 30 - figSize;

        CRCtmp = 0xffff;
        CRCtmp = crc16(CRCtmp, &etiFrame[(index - 30)], 30);
        CRCtmp ^= 0xffff;
        etiFrame[index++] = ((char *) &CRCtmp)[1];
        etiFrame[index++] = ((char *) &CRCtmp)[0];


        figSize = 0;
        // Insertion FIB 2
        if (alterneFIB < ensemble->services.size()) {
            service = ensemble->services.begin() + alterneFIB;

            // FIG type 1/1, SI, Service label, one instance per subchannel
            if ((*service)->getType(ensemble) == 0) {
                fig1_1 = (FIGtype1_1 *) & etiFrame[index];

                fig1_1->FIGtypeNumber = 1;
                fig1_1->Length = 21;
                fig1_1->Charset = 0;
                fig1_1->OE = 0;
                fig1_1->Extension = 1;

                fig1_1->Sld = htons((*service)->id);
                index += 4;
                figSize += 4;
            } else {
                fig1_5 = (FIGtype1_5 *) & etiFrame[index];
                fig1_5->FIGtypeNumber = 1;
                fig1_5->Length = 23;
                fig1_5->Charset = 0;
                fig1_5->OE = 0;
                fig1_5->Extension = 5;

                fig1_5->SId = htonl((*service)->id);
                index += 6;
                figSize += 6;
            }
            memcpy(&etiFrame[index], (*service)->label.text(), 16);
            index += 16;
            figSize += 16;
            etiFrame[index++] = (*service)->label.flag() >> 8;
            etiFrame[index++] = (*service)->label.flag() & 0xFF;
            figSize += 2;
        } else if (alterneFIB <
                ensemble->services.size() + ensemble->components.size()) {
            component = ensemble->components.begin() +
                (alterneFIB - ensemble->services.size());
            service = getService(*component, ensemble->services);
            subchannel =
                getSubchannel(ensemble->subchannels, (*component)->subchId);

            if ((*component)->label.text()[0] != 0) {
                if ((*service)->getType(ensemble) == 0) {   // Programme
                    FIGtype1_4_programme *fig1_4;
                    fig1_4 = (FIGtype1_4_programme*)&etiFrame[index];

                    fig1_4->FIGtypeNumber = 1;
                    fig1_4->Length = 22;
                    fig1_4->Charset = 0;
                    fig1_4->OE = 0;
                    fig1_4->Extension = 4;
                    fig1_4->PD = 0;
                    fig1_4->rfa = 0;
                    fig1_4->SCIdS = (*component)->SCIdS;

                    fig1_4->SId = htons((*service)->id);
                    index += 5;
                    figSize += 5;
                } else {    // Data
                    FIGtype1_4_data *fig1_4;
                    fig1_4 = (FIGtype1_4_data *) & etiFrame[index];
                    fig1_4->FIGtypeNumber = 1;
                    fig1_4->Length = 24;
                    fig1_4->Charset = 0;
                    fig1_4->OE = 0;
                    fig1_4->Extension = 4;
                    fig1_4->PD = 1;
                    fig1_4->rfa = 0;
                    fig1_4->SCIdS = (*component)->SCIdS;

                    fig1_4->SId = htonl((*service)->id);
                    index += 7;
                    figSize += 7;
                }
                memcpy(&etiFrame[index], (*component)->label.text(), 16);
                index += 16;
                figSize += 16;

                etiFrame[index++] = (*component)->label.flag() >> 8;
                etiFrame[index++] = (*component)->label.flag() & 0xFF;
                figSize += 2;
            }
        }
        memcpy(&etiFrame[index], Padding_FIB, 30 - figSize);
        index += 30 - figSize;

        CRCtmp = 0xffff;
        CRCtmp = crc16(CRCtmp, &etiFrame[(index - 30)], 30);
        CRCtmp ^= 0xffff;
        etiFrame[index++] = ((char *) &CRCtmp)[1];
        etiFrame[index++] = ((char *) &CRCtmp)[0];

        /* ETSI EN 300 799 Table 2:
         * Only TM3 has a FIB count to CIF count that is
         * not 3 to 1.
         */
        if (ensemble->mode == 3) {
            memcpy(&etiFrame[index], Padding_FIB, 30);
            index += 30;

            CRCtmp = 0xffff;
            CRCtmp = crc16(CRCtmp, &etiFrame[(index - 30)], 30);
            CRCtmp ^= 0xffff;
            etiFrame[index++] = ((char *) &CRCtmp)[1];
            etiFrame[index++] = ((char *) &CRCtmp)[0];
        }

        if (ensemble->services.size() > 30) {
            etiLog.log(error,
                    "Sorry, but this software currently can't write "
                    "Service Label of more than 30 services.\n");
            returnCode = -1;
            goto EXIT;
        }

        // compteur pour FIG 0/0
        insertFIG = (insertFIG + 1) % 8;

        // compteur pour inserer FIB a toutes les 30 trames
        alterneFIB = (alterneFIB + 1) % 30;

        /**********************************************************************
         ******  Section de lecture de donnees  *******************************
         **********************************************************************/

        //Lecture des donnees dans les fichiers d'entree
        for (subchannel = ensemble->subchannels.begin();
                subchannel != ensemble->subchannels.end();
                ++subchannel) {
            int sizeSubchannel = getSizeByte(*subchannel);
            result = (*subchannel)->input->readFrame(
                    &etiFrame[index], sizeSubchannel);
            if (result < 0) {
                etiLog.log(info, "Subchannel %d read failed at ETI frame number: %i\n", (*subchannel)->id, currentFrame);
            }
            index += sizeSubchannel;
        }


        index = (3 + fc->NST + FICL) * 4;
        for (subchannel = ensemble->subchannels.begin();
                subchannel != ensemble->subchannels.end();
                ++subchannel) {
            index += getSizeByte(*subchannel);
        }

        /******* Section EOF **************************************************/
        // End of Frame, 4 octets 
        index = (FLtmp + 1 + 1) * 4;
        eti_EOF *eof = (eti_EOF *) & etiFrame[index];

        //CRC sur le Main Stream data (MST), sur 16 bits
        index = ((fc->NST) + 2 + 1) * 4;            //position du MST
        MSTsize = ((FLtmp) - 1 - (fc->NST)) * 4;    //nb d'octets de donnees

        CRCtmp = 0xffff;
        CRCtmp = crc16(CRCtmp, &etiFrame[index], MSTsize);
        CRCtmp ^= 0xffff;
        eof->CRC = htons(CRCtmp);

        //RFU, Reserved for future use, 2 bytes, should be 0xFFFF
        eof->RFU = htons(0xFFFF);

        /******* Section TIST *************************************************/
        // TimeStamps, 24 bits + 1 octet
        index = (FLtmp + 2 + 1) * 4;
        eti_TIST *tist = (eti_TIST *) & etiFrame[index];

        if (enableTist) {
            tist->TIST = htonl(timestamp) | 0xff;
        }
        else {
            tist->TIST = htonl(0xffffff) | 0xff;
        }

        timestamp += 3 << 17;
        if (timestamp > 0xf9ffff)
        {
            timestamp -= 0xfa0000;

            // Also update MNSC time for next frame
            MNSC_increment_time = true;
        }



        /********************************************************************** 
         ***********   Section FRPD   *****************************************
         **********************************************************************/

        //Donne le nombre total d'octets utils dans la trame
        index = (FLtmp + 1 + 1 + 1 + 1) * 4;

        // Give the data to the outputs
        for (output = outputs.begin() ; output != outputs.end(); ++output) {
            if ((*output)->output->Write(etiFrame, index)
                    == -1) {
                etiLog.log(error, "Can't write to output %s://%s\n",
                        (*output)->outputProto.c_str(), (*output)->outputName.c_str());
            }
        }

#ifdef DUMP_BRIDGE
        dumpBytes(dumpData, sizeSubChannel, stderr);
#endif // DUMP_BRIDGE

#if _DEBUG
        if (currentFrame % 100 == 0) {
            if (enableTist) {
                etiLog.log(info, "ETI frame number %i Timestamp: %d + %f\n",
                        currentFrame, mnsc_time.tv_sec, 
                        (timestamp & 0xFFFFFF) / 16384000.0);
            }
            else {
                etiLog.log(info, "ETI frame number %i Time: %d, no TIST\n",
                        currentFrame, mnsc_time.tv_sec);
            }
        }
#endif

        /* Check every six seconds if the remote control is still working */
        if (rc && fc->FCT == 249 && rc->fault_detected()) {
                etiLog.level(warn) << "Detected Remote Control fault, restarting it";
                rc->restart();
        }
    }

EXIT:
    etiLog.log(debug, "exiting...\n");
    fflush(stderr);
    //fermeture des fichiers
    for (subchannel = ensemble->subchannels.begin();
            subchannel != ensemble->subchannels.end();
            ++subchannel) {
        (*subchannel)->input->close();
        delete (*subchannel)->input;
    }
    for (output = outputs.begin() ; output != outputs.end(); ++output) {
        if ((*output)->output) {
            (*output)->output->Close();
            delete ((*output)->output);
        }
    }

    for_each(ensemble->components.begin(), ensemble->components.end(), free);
    for_each(ensemble->services.begin(), ensemble->services.end(), free);
    for_each(ensemble->subchannels.begin(), ensemble->subchannels.end(), free);
    for_each(outputs.begin(), outputs.end(), free);
    ensemble->components.clear();
    ensemble->services.clear();
    ensemble->subchannels.clear();
    free(ensemble);
    outputs.clear();

    UdpSocket::clean();

    if (returnCode < 0) {
        etiLog.log(emerg, "...aborting\n");
    } else {
        etiLog.log(debug, "...done\n");
    }

    return returnCode;
}
