/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2014, 2015
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://www.opendigitalradio.org

    The Configuration parser sets up the ensemble according
     to the configuration given in a boost property tree, which
     is directly derived from a config file.

     The format of the configuration is given in doc/example.mux
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
#include "ConfigParser.h"

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include <boost/property_tree/ptree.hpp>
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
#include "ManagementServer.h"


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

/* a helper class to parse hexadecimal ids */
int hexparse(std::string input)
{
    int value;
    if (input.find("0x") == 0) {
        value = strtoll(input.c_str() + 2, NULL, 16);
    }
    else {
        value = strtoll(input.c_str(), NULL, 10);
    }

    if (errno == ERANGE) {
        throw runtime_error("hex conversion: value out of range");
    }

    return value;
}


void parse_ptree(boost::property_tree::ptree& pt,
        boost::shared_ptr<dabEnsemble> ensemble,
        boost::shared_ptr<BaseRemoteController> rc
        )
{
    using boost::property_tree::ptree;
    using boost::property_tree::ptree_error;
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

    /* Enable Logging to syslog conditionally */
    if (pt_general.get<bool>("syslog", false)) {
        etiLog.register_backend(new LogToSyslog()); // TODO don't leak the LogToSyslog backend
    }


    /******************** READ ENSEMBLE PARAMETERS *************/
    ptree pt_ensemble = pt.get_child("ensemble");

    /* Ensemble ID */
    ensemble->id = hexparse(pt_ensemble.get("id", "0"));

    /* Extended Country Code */
    ensemble->ecc = hexparse(pt_ensemble.get("ecc", "0"));

    ensemble->international_table = pt_ensemble.get("international-table", 0);

    string lto_auto = pt_ensemble.get("local-time-offset", "");
    if (lto_auto == "auto") {
        ensemble->lto_auto = true;
        ensemble->lto = 0;
    }
    else {
        double lto_hours = pt_ensemble.get("local-time-offset", 0.0);
        if (round(lto_hours * 2) != lto_hours * 2) {
            etiLog.level(error) << "Ensemble local time offset " <<
                lto_hours << "h cannot be expressed in half-hour blocks.";
            throw runtime_error("ensemble local-time-offset definition error");
        }
        if (lto_hours > 12 || lto_hours < -12) {
            etiLog.level(error) << "Ensemble local time offset " <<
                lto_hours << "h out of bounds [-12, +12].";
            throw runtime_error("ensemble local-time-offset definition error");
        }
        ensemble->lto_auto = false;
        ensemble->lto = abs(rint(lto_hours * 2));
    }

    int success = -5;
    string ensemble_label = pt_ensemble.get<string>("label");
    string ensemble_short_label(ensemble_label);
    try {
        ensemble_short_label = pt_ensemble.get<string>("shortlabel");
        success = ensemble->label.setLabel(ensemble_label, ensemble_short_label);
    }
    catch (ptree_error &e) {
        etiLog.level(warn) << "Ensemble short label undefined, "
            "truncating label " << ensemble_label;

        success = ensemble->label.setLabel(ensemble_label);
    }
    switch (success)
    {
        case 0:
            break;
        case -1:
            etiLog.level(error) << "Ensemble short label " <<
                ensemble_short_label << " is not subset of label '" <<
                ensemble_label << "'";
            throw runtime_error("ensemble label definition error");
        case -2:
            etiLog.level(error) << "Ensemble short label " <<
                ensemble_short_label << " is too long (max 8 characters)";
            throw runtime_error("ensemble label definition error");
        case -3:
            etiLog.level(error) << "Ensemble label " <<
                ensemble_label << " is too long (max 16 characters)";
            throw runtime_error("ensemble label definition error");
        default:
            etiLog.level(error) <<
                "Ensemble short label definition: program error !";
            abort();
    }


    /******************** READ SERVICES PARAMETERS *************/

    map<string, shared_ptr<DabService> > allservices;

    /* For each service, we keep a separate SCIdS counter */
    map<shared_ptr<DabService>, int> SCIdS_per_service;

    ptree pt_services = pt.get_child("services");
    for (ptree::iterator it = pt_services.begin();
            it != pt_services.end(); ++it) {
        string serviceuid = it->first;
        ptree pt_service = it->second;

        shared_ptr<DabService> service;

        bool service_already_existing = false;

        for (auto srv : ensemble->services)
        {
            if (srv->uid == serviceuid) {
                service = srv;
                service_already_existing = true;
                break;
            }
        }

        if (not service_already_existing) {
            auto new_srv = make_shared<DabService>(serviceuid);
            ensemble->services.push_back(new_srv);
            service = new_srv;
        }

        int success = -5;

        string servicelabel = pt_service.get<string>("label");
        string serviceshortlabel(servicelabel);
        try {
            serviceshortlabel = pt_service.get<string>("shortlabel");
            success = service->label.setLabel(servicelabel, serviceshortlabel);
        }
        catch (ptree_error &e) {
            etiLog.level(warn) << "Service short label undefined, "
                "truncating label " << servicelabel;

            success = service->label.setLabel(servicelabel);
        }
        switch (success)
        {
            case 0:
                break;
            case -1:
                etiLog.level(error) << "Service short label " <<
                    serviceshortlabel << " is not subset of label '" <<
                    servicelabel << "'";
                throw runtime_error("service label definition error");
            case -2:
                etiLog.level(error) << "Service short label " <<
                    serviceshortlabel << " is too long (max 8 characters)";
                throw runtime_error("service label definition error");
            case -3:
                etiLog.level(error) << "Service label " <<
                    servicelabel << " is too long (max 16 characters)";
                throw runtime_error("service label definition error");
            default:
                etiLog.level(error) <<
                    "Service short label definition: program error !";
                abort();
        }

        stringstream def_serviceid;
        def_serviceid << DEFAULT_SERVICE_ID + ensemble->services.size();
        service->id = hexparse(pt_service.get("id", def_serviceid.str()));
        service->pty = hexparse(pt_service.get("pty", "0"));
        service->language = hexparse(pt_service.get("language", "0"));

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
    for (ptree::iterator it = pt_subchans.begin(); it != pt_subchans.end(); ++it) {
        string subchanuid = it->first;
        dabSubchannel* subchan = new dabSubchannel(subchanuid);

        ensemble->subchannels.push_back(subchan);

        try {
            setup_subchannel_from_ptree(subchan, it->second, ensemble,
                    subchanuid, rc);
        }
        catch (runtime_error &e) {
            etiLog.log(error,
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
    map<string, DabComponent*> allcomponents;
    ptree pt_components = pt.get_child("components");
    for (ptree::iterator it = pt_components.begin(); it != pt_components.end(); ++it) {
        string componentuid = it->first;
        ptree pt_comp = it->second;

        shared_ptr<DabService> service;
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

        int figType = hexparse(pt_comp.get("figtype", "-1"));
        int packet_address = hexparse(pt_comp.get("address", "-1"));
        int packet_datagroup = pt_comp.get("datagroup", false);
        uint8_t component_type = hexparse(pt_comp.get("type", "0"));

        DabComponent* component = new DabComponent(componentuid);

        component->serviceId = service->id;
        component->subchId = subchannel->id;
        component->SCIdS = SCIdS_per_service[service]++;
        component->type = component_type;

        int success = -5;
        string componentlabel = pt_comp.get("label", "");
        string componentshortlabel(componentlabel);
        try {
            componentshortlabel = pt_comp.get<string>("shortlabel");
            success = component->label.setLabel(componentlabel, componentshortlabel);
        }
        catch (ptree_error &e) {
            if (not componentlabel.empty()) {
                etiLog.level(warn) << "Component short label undefined, "
                    "truncating label " << componentlabel;
            }

            success = component->label.setLabel(componentlabel);
        }
        switch (success)
        {
            case 0:
                break;
            case -1:
                etiLog.level(error) << "Component short label " <<
                    componentshortlabel << " is not subset of label '" <<
                    componentlabel << "'";
                throw runtime_error("component label definition error");
            case -2:
                etiLog.level(error) << "Component short label " <<
                    componentshortlabel << " is too long (max 8 characters)";
                throw runtime_error("component label definition error");
            case -3:
                etiLog.level(error) << "Component label " <<
                    componentlabel << " is too long (max 16 characters)";
                throw runtime_error("component label definition error");
            default:
                etiLog.level(error) <<
                    "Component short label definition: program error !";
                abort();
        }

        if (figType != -1) {
            if (figType >= (1<<12)) {
                stringstream ss;
                ss << "Component with uid " << componentuid <<
                    ": figtype '" << figType << "' is too large !";
                throw runtime_error(ss.str());
            }

            if (component->isPacketComponent(ensemble->subchannels)) {
                component->packet.appType = figType;

            }
            else {
                component->audio.uaType = figType;
            }

            if (packet_address != -1) {
                if (! component->isPacketComponent(ensemble->subchannels)) {
                    stringstream ss;
                    ss << "Component with uid " << componentuid <<
                        " is not packet, cannot have address defined !";
                    throw runtime_error(ss.str());
                }

                component->packet.address = packet_address;
            }
            if (packet_datagroup) {
                if (! component->isPacketComponent(ensemble->subchannels)) {
                    stringstream ss;
                    ss << "Component with uid " << componentuid <<
                        " is not packet, cannot have datagroup enabled !";
                    throw runtime_error(ss.str());
                }

                component->packet.datagroup = packet_datagroup;
            }

        }

        ensemble->components.push_back(component);

    }

}

void setup_subchannel_from_ptree(dabSubchannel* subchan,
        boost::property_tree::ptree &pt,
        boost::shared_ptr<dabEnsemble> ensemble,
        string subchanuid,
        boost::shared_ptr<BaseRemoteController> rc)
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

    string inputUri = "";
    // fail if no inputUri given unless type is test
    if (type != "test") {
        inputUri = pt.get<string>("inputuri", "");

        if (inputUri == "") {
            try {
                inputUri = pt.get<string>("inputfile");
            }
            catch (ptree_error &e) {
                stringstream ss;
                ss << "Subchannel with uid " << subchanuid << " has no inputUri defined!";
                throw runtime_error(ss.str());
            }
        }
    }

    string proto;
    size_t protopos = inputUri.find("://");
    if (protopos == string::npos) {
        proto = "file";
    }
    else {
        proto = inputUri.substr(0, protopos);
    }

    subchan->inputUri = inputUri;

    /* The input is of the old_style type,
     * with the struct of function pointers,
     * and needs to be a DabInputCompatible
     */
    bool input_is_old_style = true;
    dabInputOperations operations;
    dabProtection* protection = &subchan->protection;


    if (0) {
#if defined(HAVE_FORMAT_MPEG)
    } else if (type == "audio") {
        subchan->type = subchannel_type_t::Audio;
        subchan->bitrate = 0;

        if (0) {
#if defined(HAVE_INPUT_FILE)
        } else if (proto == "file") {
            operations = dabInputMpegFileOperations;
#endif // defined(HAVE_INPUT_FILE)
#if defined(HAVE_INPUT_ZEROMQ)
        }
        else if (proto == "tcp"  ||
                 proto == "epmg" ||
                 proto == "ipc") {
            input_is_old_style = false;

            dab_input_zmq_config_t zmqconfig;

            try {
                zmqconfig.buffer_size = pt.get<int>("zmq-buffer");
            }
            catch (ptree_error &e) {
                stringstream ss;
                ss << "ZMQ Subchannel with uid " << subchanuid <<
                    " has no zmq-buffer defined!";
                throw runtime_error(ss.str());
            }
            try {
                zmqconfig.prebuffering = pt.get<int>("zmq-prebuffering");
            }
            catch (ptree_error &e) {
                stringstream ss;
                ss << "ZMQ Subchannel with uid " << subchanuid <<
                    " has no zmq-buffer defined!";
                throw runtime_error(ss.str());
            }
            zmqconfig.enable_encryption = false;

            DabInputZmqMPEG* inzmq =
                new DabInputZmqMPEG(subchanuid, zmqconfig);
            inzmq->enrol_at(*rc);
            subchan->input     = inzmq;

            if (proto == "epmg") {
                etiLog.level(warn) << "Using untested epmg:// zeromq input";
            }
            else if (proto == "ipc") {
                etiLog.level(warn) << "Using untested ipc:// zeromq input";
            }
#endif // defined(HAVE_INPUT_ZEROMQ)
        } else {
            stringstream ss;
            ss << "Subchannel with uid " << subchanuid <<
                ": Invalid protocol for MPEG input (" <<
                proto << ")" << endl;
            throw runtime_error(ss.str());
        }
#endif // defined(HAVE_INPUT_FILE) && defined(HAVE_FORMAT_MPEG)
#if defined(HAVE_FORMAT_DABPLUS)
    } else if (type == "dabplus") {
        subchan->type = subchannel_type_t::Audio;
        subchan->bitrate = 32;

        if (0) {
#if defined(HAVE_INPUT_FILE)
        } else if (proto == "file") {
            operations = dabInputDabplusFileOperations;
#endif // defined(HAVE_INPUT_FILE)
#if defined(HAVE_INPUT_ZEROMQ)
        }
        else if (proto == "tcp"  ||
                 proto == "epmg" ||
                 proto == "ipc") {
            input_is_old_style = false;

            dab_input_zmq_config_t zmqconfig;

            try {
                zmqconfig.buffer_size = pt.get<int>("zmq-buffer");
            }
            catch (ptree_error &e) {
                stringstream ss;
                ss << "ZMQ Subchannel with uid " << subchanuid <<
                    " has no zmq-buffer defined!";
                throw runtime_error(ss.str());
            }

            try {
                zmqconfig.prebuffering = pt.get<int>("zmq-prebuffering");
            }
            catch (ptree_error &e) {
                stringstream ss;
                ss << "ZMQ Subchannel with uid " << subchanuid <<
                    " has no zmq-buffer defined!";
                throw runtime_error(ss.str());
            }

            zmqconfig.curve_encoder_keyfile = pt.get<string>("encoder-key","");
            zmqconfig.curve_secret_keyfile = pt.get<string>("secret-key","");
            zmqconfig.curve_public_keyfile = pt.get<string>("public-key","");

            zmqconfig.enable_encryption = pt.get<int>("encryption", 0);

            DabInputZmqAAC* inzmq =
                new DabInputZmqAAC(subchanuid, zmqconfig);

            inzmq->enrol_at(*rc);
            subchan->input     = inzmq;

            if (proto == "epmg") {
                etiLog.level(warn) << "Using untested epmg:// zeromq input";
            }
            else if (proto == "ipc") {
                etiLog.level(warn) << "Using untested ipc:// zeromq input";
            }
#endif // defined(HAVE_INPUT_ZEROMQ)
        } else {
            stringstream ss;
            ss << "Subchannel with uid " << subchanuid <<
                ": Invalid protocol for DAB+ input (" <<
                proto << ")" << endl;
            throw runtime_error(ss.str());
        }
#endif // defined(HAVE_FORMAT_DABPLUS)
    } else if (type == "bridge") {
        // TODO default proto should be udp://
        if (0) {
#if defined(HAVE_FORMAT_BRIDGE)
#if defined(HAVE_INPUT_UDP)
        } else if (proto == "udp") {
            operations = dabInputBridgeUdpOperations;
#endif // defined(HAVE_INPUT_UDP)
#if defined(HAVE_INPUT_SLIP)
        } else if (proto == "slip") {
            operations = dabInputSlipOperations;
#endif // defined(HAVE_INPUT_SLIP)
#endif // defined(HAVE_FORMAT_BRIDGE)
        }
    } else if (type == "data") {
        // TODO default proto should be udp://
        if (0) {
#if defined(HAVE_INPUT_UDP)
        } else if (proto == "udp") {
            operations = dabInputUdpOperations;
#endif
#if defined(HAVE_INPUT_PRBS) && defined(HAVE_FORMAT_RAW)
        } else if (proto == "prbs") {
            operations = dabInputPrbsOperations;
#endif
#if defined(HAVE_INPUT_FILE) && defined(HAVE_FORMAT_RAW)
        } else if (proto == "file") {
            operations = dabInputRawFileOperations;
#endif
        } else if (proto == "fifo") {
            operations = dabInputRawFifoOperations;
        } else {
            stringstream ss;
            ss << "Subchannel with uid " << subchanuid << 
                ": Invalid protocol for data input (" <<
                proto << ")" << endl;
            throw runtime_error(ss.str());
        }

        subchan->type = subchannel_type_t::DataDmb;
        subchan->bitrate = DEFAULT_DATA_BITRATE;
#if defined(HAVE_INPUT_TEST) && defined(HAVE_FORMAT_RAW)
    } else if (type == "test") {
        subchan->type = DataDmb;
        subchan->bitrate = DEFAULT_DATA_BITRATE;
        operations = dabInputTestOperations;
#endif // defined(HAVE_INPUT_TEST)) && defined(HAVE_FORMAT_RAW)
#ifdef HAVE_FORMAT_PACKET
    } else if (type == "packet") {
        subchan->type = subchannel_type_t::Packet;
        subchan->bitrate = DEFAULT_PACKET_BITRATE;
#ifdef HAVE_INPUT_FILE
        operations = dabInputPacketFileOperations;
#elif defined(HAVE_INPUT_FIFO)
        operations = dabInputFifoOperations;
#else
#   pragma error("Must define at least one packet input")
#endif // defined(HAVE_INPUT_FILE)
#ifdef HAVE_FORMAT_EPM
    } else if (type == "enhancedpacked") {
        subchan->type = subchannel_type_t::Packet;
        subchan->bitrate = DEFAULT_PACKET_BITRATE;
        operations = dabInputEnhancedPacketFileOperations;
#endif // defined(HAVE_FORMAT_EPM)
#endif // defined(HAVE_FORMAT_PACKET)
#ifdef HAVE_FORMAT_DMB
    } else if (type == "dmb") {
        // TODO default proto should be UDP
        if (0) {
#if defined(HAVE_INPUT_UDP)
        } else if (proto == "udp") {
            operations = dabInputDmbUdpOperations;
#endif
        } else if (proto == "file") {
            operations = dabInputDmbFileOperations;
        } else {
            stringstream ss;
            ss << "Subchannel with uid " << subchanuid << 
                ": Invalid protocol for DMB input (" <<
                proto << ")" << endl;
            throw runtime_error(ss.str());
        }

        subchan->type = subchannel_type_t::DataDmb;
        subchan->bitrate = DEFAULT_DATA_BITRATE;
#endif
    } else {
        stringstream ss;
        ss << "Subchannel with uid " << subchanuid << " has unknown type!";
        throw runtime_error(ss.str());
    }
    subchan->startAddress = 0;

    if (type == "audio") {
        protection->form = UEP;
        protection->level = 2;
        protection->uep.tableIndex = 0;
    } else {
        protection->level = 2;
        protection->form = EEP;
        protection->eep.profile = EEP_A;
    }

    /* Get bitrate */
    try {
        subchan->bitrate = pt.get<int>("bitrate");
        if ((subchan->bitrate & 0x7) != 0) {
            stringstream ss;
            ss << "Subchannel with uid " << subchanuid <<
                ": Bitrate (" << subchan->bitrate << " not a multiple of 8!";
            throw runtime_error(ss.str());
        }
    }
    catch (ptree_error &e) {
        stringstream ss;
        ss << "Error, no bitrate defined for subchannel " << subchanuid;
        throw runtime_error(ss.str());
    }

#if defined(HAVE_INPUT_FIFO) && defined(HAVE_INPUT_FILE)
    /* Get nonblock */
    bool nonblock = pt.get("nonblock", false);
    if (nonblock) {
        switch (subchan->type) {
#ifdef HAVE_FORMAT_PACKET
            case subchannel_type_t::Packet:
                if (operations == dabInputPacketFileOperations) {
                    operations = dabInputFifoOperations;
#ifdef HAVE_FORMAT_EPM
                } else if (operations == dabInputEnhancedPacketFileOperations) {
                    operations = dabInputEnhancedFifoOperations;
#endif // defined(HAVE_FORMAT_EPM)
                } else {
                    stringstream ss;
                    ss << "Error, wrong packet operations for subchannel " <<
                        subchanuid;
                    throw runtime_error(ss.str());
                }
                break;
#endif // defined(HAVE_FORMAT_PACKET)
#ifdef HAVE_FORMAT_MPEG
            case subchannel_type_t::Audio:
                if (operations == dabInputMpegFileOperations) {
                    operations = dabInputMpegFifoOperations;
                } else if (operations == dabInputDabplusFileOperations) {
                    operations = dabInputDabplusFifoOperations;
                } else {
                    stringstream ss;
                    ss << "Error, wrong audio operations for subchannel " <<
                        subchanuid;
                    throw runtime_error(ss.str());
                }
                break;
#endif // defined(HAVE_FORMAT_MPEG)
            case subchannel_type_t::DataDmb:
            case subchannel_type_t::Fidc:
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
        subchan->id = hexparse(pt.get<std::string>("subchid"));
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

    /* Get optional protection profile */
    string profile = pt.get("protection-profile", "");

    if (profile == "EEP_A") {
        protection->form = EEP;
        protection->eep.profile = EEP_A;
    }
    else if (profile == "EEP_B") {
        protection->form = EEP;
        protection->eep.profile = EEP_B;
    }
    else if (profile == "UEP") {
        protection->form = UEP;
    }

    /* Get protection level */
    try {
        int level = pt.get<int>("protection");

        if (protection->form == UEP) {
            if ((level < 1) || (level > 5)) {
                stringstream ss;
                ss << "Subchannel with uid " << subchanuid <<
                    ": protection level must be between "
                    "1 to 5 inclusively (current = " << level << " )";
                throw runtime_error(ss.str());
            }
        }
        else if (protection->form == EEP) {
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
    catch (ptree_error &e) {
        stringstream ss;
        ss << "Subchannel with uid " << subchanuid <<
            ": protection level undefined!";
        throw runtime_error(ss.str());
    }

    /* Create object */
    if (input_is_old_style) {
        subchan->input = new DabInputCompatible(operations);
    }
    // else { it's already been created! }
}

