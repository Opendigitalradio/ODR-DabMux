/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Written by
   Matthias P. Braendli, matthias.braendli@mpb.li, 2012

   The command-line parser reads settings from a configuration file
   whose definition is given in doc/example.config
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
#include "ParserConfigfile.h"

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/info_parser.hpp>
#include <exception>
#include <iostream>
#include <vector>
#include <stdint.h>
#include <string>
#include <map>
#include <cstring>
#include "dabOutput/dabOutput.h"
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
#include "dabInputZmq.h"
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

using namespace std;

void set_short_label(dabLabel& label, std::string& slabel, const char* applies_to)
{
    char* end;
    const char* lab;
    label.flag = strtoul(slabel.c_str(), &end, 0);
    if (*end != 0) {
        lab = slabel.c_str();
        label.flag = 0;
        for (int i = 0; i < 32; ++i) {
            if (*lab == label.text[i]) {
                label.flag |= 0x8000 >> i;
                if (*(++lab) == 0) {
                    break;
                }
            }
        }
        if (*lab != 0) {
            stringstream ss;
            ss << "Error : " << applies_to << " short label '" << slabel <<
                "'!\n" << "Not in label '" << label.text << "' !\n";
            throw runtime_error(ss.str());
        }
    }
    int count = 0;
    for (int i = 0; i < 16; ++i) {
        if (label.flag & (1 << i)) {
            ++count;
        }
    }
    if (count > 8) {
        stringstream ss;
        ss << applies_to << " '" << slabel << "' short label too long!\n"
            "Must be < 8 characters.\n";
        throw runtime_error(ss.str());
    }

}

void parse_configfile(string configuration_file,
        vector<dabOutput*> &outputs,
        dabEnsemble* ensemble,
        bool* enableTist,
        unsigned* FICL,
        bool* factumAnalyzer,
        unsigned long* limit
        )
{
    using boost::property_tree::ptree;
    using boost::property_tree::ptree_error;
    ptree pt;

    read_info(configuration_file, pt);
    /******************** READ GENERAL OPTIONS *****************/
    ptree pt_general = pt.get_child("general");

    /* Dab mode logic */
    ensemble->mode = pt_general.get("dabmode", 2);
    if ((ensemble->mode < 1) || (ensemble->mode > 4)) {
        throw runtime_error("Mode must be between 1-4");
    }
    if (ensemble->mode == 4) {
        ensemble->mode = 0;
    }

    if (ensemble->mode == 3) {
        *FICL = 32;
    }
    else {
        *FICL = 24;
    }

    /* Number of frames to generate */
    *limit = pt_general.get("nbframes", 0);

    /* Enable TCPLog conditionally */
    if (pt_general.get("tcplog", 0)) {
        etiLog.open("createETI", 0, 12222);
    }

    *factumAnalyzer = pt_general.get("writescca", false);

    *enableTist = pt_general.get("tist", false);

    /******************** READ ENSEMBLE PARAMETERS *************/
    ptree pt_ensemble = pt.get_child("ensemble");

    /* Ensemble ID */
    ensemble->id = pt_ensemble.get("id", 0);

    /* Extended Country Code */
    ensemble->ecc = pt_ensemble.get("ecc", 0);

    string label = pt_ensemble.get("label", "");
    memset(ensemble->label.text, 0, 16);
    label.copy(ensemble->label.text, 16);
    ensemble->label.flag = 0xff00;

    try {
        string label = pt_ensemble.get<string>("shortlabel");
        set_short_label(ensemble->label, label, "Ensemble");
    }
    catch (ptree_error &e) { }


    /******************** READ SERVICES PARAMETERS *************/

    map<string, dabService*> allservices;

    /* For each service, we keep a separate SCIdS counter */
    map<dabService*, int> SCIdS_per_service;

    ptree pt_services = pt.get_child("services");
    for (ptree::iterator it = pt_services.begin();
            it != pt_services.end(); it++) {
        string serviceuid = it->first;
        ptree pt_service = it->second;
        dabService* service = new dabService();
        ensemble->services.push_back(service);

        memset(service->label.text, 0, 16);
        try {
            string servicelabel = pt_service.get<string>("label");
            servicelabel.copy(service->label.text, 16);
        }
        catch (ptree_error &e)
        {
            sprintf(service->label.text, "CRC-Service%i",
                    (int)ensemble->services.size());
        }
        service->label.flag = 0xe01f;

        try {
            string label = pt_service.get<string>("shortlabel");
            set_short_label(service->label, label, "Service");
        }
        catch (ptree_error &e) {
            etiLog.printHeader(TcpLog::WARNING,
                    "Service with uid %s has no short label.\n", serviceuid.c_str());
        }


        service->id = pt_service.get("id", DEFAULT_SERVICE_ID + ensemble->services.size());
        service->pty = pt_service.get("pty", 0);
        service->language = pt_service.get("language", 0);

        // keep service in map, and check for uniqueness of the UID
        if (allservices.count(serviceuid) == 0) {
            allservices[serviceuid] = service;

            // Set the service's SCIds to zero
            SCIdS_per_service[service] = 0;
        }
        else {
            stringstream ss;
            ss << "Service with uid " << serviceuid << " not unique!";
            throw runtime_error(ss.str());
        }
    }


    /******************** READ SUBCHAN PARAMETERS **************/
    map<string, dabSubchannel*> allsubchans;

    ptree pt_subchans = pt.get_child("subchannels");
    for (ptree::iterator it = pt_subchans.begin(); it != pt_subchans.end(); it++) {
        string subchanuid = it->first;
        dabSubchannel* subchan = new dabSubchannel();

        ensemble->subchannels.push_back(subchan);

        try {
            setup_subchannel_from_ptree(subchan, it->second, ensemble, subchanuid);
        }
        catch (runtime_error &e) {
            etiLog.printHeader(TcpLog::ERR,
                    "%s\n", e.what());
            throw e;
        }


        // keep subchannels in map, and check for uniqueness of the UID
        if (allsubchans.count(subchanuid) == 0) {
            allsubchans[subchanuid] = subchan;
        }
        else {
            stringstream ss;
            ss << "Subchannel with uid " << subchanuid << " not unique!";
            throw runtime_error(ss.str());
        }
    }

    /******************** READ COMPONENT PARAMETERS ************/
    map<string, dabComponent*> allcomponents;
    ptree pt_components = pt.get_child("components");
    for (ptree::iterator it = pt_components.begin(); it != pt_components.end(); it++) {
        string componentuid = it->first;
        ptree pt_comp = it->second;

        dabService* service;
        try {
            // Those two uids serve as foreign keys to select the service+subchannel
            string service_uid = pt_comp.get<string>("service");
            if (allservices.count(service_uid) != 1) {
                stringstream ss;
                ss << "Component with uid " << componentuid << " is refers to unknown service "
                    << service_uid << " !";
                throw runtime_error(ss.str());
            }
            service = allservices[service_uid];
        }
        catch (ptree_error &e) {
            stringstream ss;
            ss << "Component with uid " << componentuid << " is missing service definition!";
            throw runtime_error(ss.str());
        }

        dabSubchannel* subchannel;
        try {
            string subchan_uid = pt_comp.get<string>("subchannel");
            if (allsubchans.count(subchan_uid) != 1) {
                stringstream ss;
                ss << "Component with uid " << componentuid << " is refers to unknown subchannel "
                    << subchan_uid << " !";
                throw runtime_error(ss.str());
            }
            subchannel = allsubchans[subchan_uid];
        }
        catch (ptree_error &e) {
            stringstream ss;
            ss << "Component with uid " << componentuid << " is missing subchannel definition!";
            throw runtime_error(ss.str());
        }

        dabComponent* component = new dabComponent();

        memset(component, 0, sizeof(dabComponent));
        memset(component->label.text, 0, 16);
        component->label.flag = 0xffff;
        component->serviceId = service->id;
        component->subchId = subchannel->id;
        component->SCIdS = SCIdS_per_service[service]++;

        try {
            string label = pt_comp.get<string>("label");

            memset(component->label.text, 0, 16);
            label.copy(component->label.text, 16);
            component->label.flag = 0xff00;
        }
        catch (ptree_error &e) {
            etiLog.printHeader(TcpLog::WARNING,
                    "Service with uid %s has no label.\n", componentuid.c_str());
        }

        try {
            string label = pt_comp.get<string>("shortlabel");
            set_short_label(component->label, label, "Component");
        }
        catch (ptree_error &e) {
            etiLog.printHeader(TcpLog::WARNING,
                    "Component with uid %s has no short label.\n", componentuid.c_str());
        }

        ensemble->components.push_back(component);

    }


    /******************** READ OUTPUT PARAMETERS ***************/
    map<string, dabOutput*> alloutputs;
    ptree pt_outputs = pt.get_child("outputs");
    for (ptree::iterator it = pt_outputs.begin(); it != pt_outputs.end(); it++) {
        string outputuid = it->first;
        string uri = pt_outputs.get<string>(outputuid);

        int proto_pos = uri.find("://");
        if (proto_pos == std::string::npos) {
            stringstream ss;
            ss << "Output with uid " << outputuid << " no protocol defined!";
            throw runtime_error(ss.str());
        }

        char* uri_c = new char[512];
        memset(uri_c, 0, 512);
        uri.copy(uri_c, 511);

        uri_c[proto_pos] = '\0';

        char* outputName = uri_c + proto_pos + 3;

        dabOutput* output = new dabOutput(uri_c, outputName);
        outputs.push_back(output);

        // keep outputs in map, and check for uniqueness of the uid
        if (alloutputs.count(outputuid) == 0) {
            alloutputs[outputuid] = output;
        }
        else {
            stringstream ss;
            ss << "output with uid " << outputuid << " not unique!";
            throw runtime_error(ss.str());
        }
    }

}

void setup_subchannel_from_ptree(dabSubchannel* subchan,
        boost::property_tree::ptree &pt,
        dabEnsemble* ensemble,
        string subchanuid)
{
    using boost::property_tree::ptree;
    using boost::property_tree::ptree_error;

    string type;
    /* Read type first */
    try {
        type = pt.get<string>("type");
    }
    catch (ptree_error &e) {
        stringstream ss;
        ss << "Subchannel with uid " << subchanuid << " has no type defined!";
        throw runtime_error(ss.str());
    }

    string inputfile = "";
    // fail if no inputfile given unless type is test
    if (type != "test") {
        try {
            inputfile = pt.get<string>("inputfile");
        }
        catch (ptree_error &e) {
            stringstream ss;
            ss << "Subchannel with uid " << subchanuid << " has no type defined!";
            throw runtime_error(ss.str());
        }
    }

    char* inputName = new char[512];
    memset(inputName, 0, 512);
    inputfile.copy(inputName, 511);

    subchan->inputName = inputName;


    dabProtection* protection = &subchan->protection;

    if (0) {
#if defined(HAVE_INPUT_FILE) && defined(HAVE_FORMAT_MPEG)
    } else if (type == "audio") {
        subchan->inputProto = "file";
        subchan->type = 0;
        subchan->bitrate = 0;
        subchan->operations = dabInputMpegFileOperations;
#endif // defined(HAVE_INPUT_FILE) && defined(HAVE_FORMAT_MPEG)
#if defined(HAVE_FORMAT_DABPLUS)
    } else if (type == "dabplus") {
        subchan->type = 0;
        subchan->bitrate = 32;

        char* proto;

        proto = strstr(inputName, "://");
        if (proto == NULL) {
            subchan->inputProto = "file";
        } else {
            subchan->inputProto = inputName;
            subchan->inputName = proto + 3;
            *proto = 0;
        }

        if (0) {
#if defined(HAVE_INPUT_FILE)
        } else if (strcmp(subchan->inputProto, "file") == 0) {
            subchan->operations = dabInputDabplusFileOperations;
#endif // defined(HAVE_INPUT_FILE)
#if defined(HAVE_INPUT_ZEROMQ)
        }
        else if (strcmp(subchan->inputProto, "tcp") == 0) {
            subchan->operations = dabInputZmqOperations;
        }
        else if (strcmp(subchan->inputProto, "epmg") == 0) {
            etiLog.printHeader(TcpLog::WARNING,
                    "Using untested epmg:// zeromq input\n");
            subchan->operations = dabInputZmqOperations;
        }
        else if (strcmp(subchan->inputProto, "ipc") == 0) {
            etiLog.printHeader(TcpLog::WARNING,
                    "Using untested ipc:// zeromq input\n");
            subchan->operations = dabInputZmqOperations;
#endif // defined(HAVE_INPUT_ZEROMQ)
        } else {
            stringstream ss;
            ss << "Subchannel with uid " << subchanuid <<
                ": Invalid protocol for DAB+ input (" <<
                subchan->inputProto << ")" << endl;
            throw runtime_error(ss.str());
        }
#endif // defined(HAVE_FORMAT_DABPLUS)
    } else if (type == "bridge") {
        char* proto;

        proto = strstr(inputName, "://");
        if (proto == NULL) {
            subchan->inputProto = "udp";
        } else {
            subchan->inputProto = inputName;
            subchan->inputName = proto + 3;
            *proto = 0;
        }
        if (0) {
#if defined(HAVE_FORMAT_BRIDGE)
#if defined(HAVE_INPUT_UDP)
        } else if (strcmp(subchan->inputProto, "udp") == 0) {
            subchan->operations = dabInputBridgeUdpOperations;
#endif // defined(HAVE_INPUT_UDP)
#if defined(HAVE_INPUT_SLIP)
        } else if (strcmp(subchan->inputProto, "slip") == 0) {
            subchan->operations = dabInputSlipOperations;
#endif // defined(HAVE_INPUT_SLIP)
#endif // defined(HAVE_FORMAT_BRIDGE)
        }
    } else if (type == "data") {
        char* proto;

        proto = strstr(inputName, "://");
        if (proto == NULL) {
            subchan->inputProto = "udp";
        } else {
            subchan->inputProto = inputName;
            subchan->inputName = proto + 3;
            *proto = 0;
        }
        if (0) {
#if defined(HAVE_INPUT_UDP)
        } else if (strcmp(subchan->inputProto, "udp") == 0) {
            subchan->operations = dabInputUdpOperations;
#endif
#if defined(HAVE_INPUT_PRBS) && defined(HAVE_FORMAT_RAW)
        } else if (strcmp(subchan->inputProto, "prbs") == 0) {
            subchan->operations = dabInputPrbsOperations;
#endif
#if defined(HAVE_INPUT_FILE) && defined(HAVE_FORMAT_RAW)
        } else if (strcmp(subchan->inputProto, "file") == 0) {
            subchan->operations = dabInputRawFileOperations;
#endif
        } else if (strcmp(subchan->inputProto, "fifo") == 0) {
            subchan->operations = dabInputRawFifoOperations;
        } else {
            stringstream ss;
            ss << "Subchannel with uid " << subchanuid << 
                ": Invalid protocol for data input (" <<
                subchan->inputProto << ")" << endl;
            throw runtime_error(ss.str());
        }

        subchan->type = 1;
        subchan->bitrate = DEFAULT_DATA_BITRATE;
#if defined(HAVE_INPUT_TEST) && defined(HAVE_FORMAT_RAW)
    } else if (type == "test") {
        subchan->inputProto = "test";
        subchan->type = 1;
        subchan->bitrate = DEFAULT_DATA_BITRATE;
        subchan->operations = dabInputTestOperations;
#endif // defined(HAVE_INPUT_TEST)) && defined(HAVE_FORMAT_RAW)
#ifdef HAVE_FORMAT_PACKET
    } else if (type == "packet") {
        subchan->inputProto = "file";
        subchan->type = 3;
        subchan->bitrate = DEFAULT_PACKET_BITRATE;
#ifdef HAVE_INPUT_FILE
        subchan->operations = dabInputPacketFileOperations;
#elif defined(HAVE_INPUT_FIFO)
        subchan->operations = dabInputFifoOperations;
#else
#   pragma error("Must defined at least one packet input")
#endif // defined(HAVE_INPUT_FILE)
#ifdef HAVE_FORMAT_EPM
    } else if (type == "enhancedpacked") {
        subchan->inputProto = "file";
        subchan->type = 3;
        subchan->bitrate = DEFAULT_PACKET_BITRATE;
        subchan->operations = dabInputEnhancedPacketFileOperations;
#endif // defined(HAVE_FORMAT_EPM)
#endif // defined(HAVE_FORMAT_PACKET)
#ifdef HAVE_FORMAT_DMB
    } else if (type == "dmb") {
        char* proto;

        proto = strstr(inputName, "://");
        if (proto == NULL) {
            subchan->inputProto = "udp";
        } else {
            subchan->inputProto = inputName;
            subchan->inputName = proto + 3;
            *proto = 0;
        }
        if (strcmp(subchan->inputProto, "udp") == 0) {
            subchan->operations = dabInputDmbUdpOperations;
        } else if (strcmp(subchan->inputProto, "file") == 0) {
            subchan->operations = dabInputDmbFileOperations;
        } else {
            stringstream ss;
            ss << "Subchannel with uid " << subchanuid << 
                ": Invalid protocol for DMB input (" <<
                subchan->inputProto << ")" << endl;
            throw runtime_error(ss.str());
        }

        subchan->type = 1;
        subchan->bitrate = DEFAULT_DATA_BITRATE;
#endif
    } else {
        stringstream ss;
        ss << "Subchannel with uid " << subchanuid << " has unknown type!";
        throw runtime_error(ss.str());
    }
    subchan->operations.init(&subchan->data);
    subchan->startAddress = 0;

    if (type == "audio") {
        protection->form = 0;
        protection->level = 2;
        protection->shortForm.tableSwitch = 0;
        protection->shortForm.tableIndex = 0;
    } else {
        protection->level = 2;
        protection->form = 1;
        protection->longForm.option = 0;
    }

    /* Get bitrate */
    try {
        subchan->bitrate = pt.get<int>("bitrate");
        if ((subchan->bitrate & 0x7) != 0) {
            stringstream ss;
            ss << "Subchannel with uid " << subchanuid << ": Bitrate (" << subchan->bitrate << " not a multiple of 8!";
            throw runtime_error(ss.str());
        }
    }
    catch (ptree_error &e) {}

#if defined(HAVE_INPUT_FIFO) && defined(HAVE_INPUT_FILE)
    /* Get nonblock */
    bool nonblock = pt.get("nonblock", false);
    if (nonblock) {
        switch (subchan->type) {
#ifdef HAVE_FORMAT_PACKET
            case 3:
                subchan->operations.clean(&subchan->data);
                if (subchan->operations == dabInputPacketFileOperations) {
                    subchan->operations = dabInputFifoOperations;
#ifdef HAVE_FORMAT_EPM
                } else if (subchan->operations == dabInputEnhancedPacketFileOperations) {
                    subchan->operations = dabInputEnhancedFifoOperations;
#endif // defined(HAVE_FORMAT_EPM)
                } else {
                    etiLog.printHeader(TcpLog::ERR,
                            "Error, wrong packet subchannel operations!\n");
                    throw runtime_error("Error, wrong packet subchannel operations!\n");
                }
                subchan->operations.init(&subchan->data);
                subchan->inputProto = "fifo";
                break;
#endif // defined(HAVE_FORMAT_PACKET)
#ifdef HAVE_FORMAT_MPEG
            case 0:
                subchan->operations.clean(&subchan->data);
                if (subchan->operations == dabInputMpegFileOperations) {
                    subchan->operations = dabInputMpegFifoOperations;
                } else if (subchan->operations ==
                        dabInputDabplusFileOperations) {
                    subchan->operations = dabInputDabplusFifoOperations;
                } else {
                    etiLog.printHeader(TcpLog::ERR,
                            "Error, wrong audio subchannel operations!\n");
                    throw runtime_error(
                            "Error, wrong audio subchannel operations!\n");
                }
                subchan->operations.init(&subchan->data);
                subchan->inputProto = "fifo";
                break;
#endif // defined(HAVE_FORMAT_MPEG)
            default:
                stringstream ss;
                ss << "Subchannel with uid " << subchanuid <<
                    " non-blocking I/O only available for audio or packet services!";
                throw runtime_error(ss.str());
        }
#endif // defined(HAVE_INPUT_FIFO) && defined(HAVE_INPUT_FILE)
    }


    /* Get id */

    try {
        subchan->id = pt.get<int>("subchid");
    }
    catch (ptree_error &e) {
        for (int i = 0; i < 64; ++i) { // Find first free subchannel
            vector<dabSubchannel*>::iterator subchannel = getSubchannel(ensemble->subchannels, i);
            if (subchannel == ensemble->subchannels.end()) {
                subchannel = ensemble->subchannels.end() - 1;
                subchan->id = i;
                break;
            }
        }
    }

    /* Get protection */
    try {
        int level = pt.get<int>("protection");

        if (protection->form == 0) {
            if ((level < 1) || (level > 5)) {
                stringstream ss;
                ss << "Subchannel with uid " << subchanuid <<
                    ": protection level must be between "
                    "1 to 5 inclusively (current = " << level << " )";
                throw runtime_error(ss.str());
            }
        }
        else {
            if ((level < 1) || (level > 4)) {
                stringstream ss;
                ss << "Subchannel with uid " << subchanuid <<
                    ": protection level must be between "
                    "1 to 4 inclusively (current = " << level << " )";
                throw runtime_error(ss.str());
            }
        }
        protection->level = level - 1;
    }
    catch (ptree_error &e) {}

}
