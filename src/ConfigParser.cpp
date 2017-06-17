/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2017
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
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <memory>
#include <exception>
#include <iostream>
#include <vector>
#include <stdint.h>
#include <string>
#include <map>
#include <cstring>
#include "dabOutput/dabOutput.h"
#include "input/inputs.h"
#include "utils.h"
#include "DabMux.h"
#include "ManagementServer.h"

#include "input/Prbs.h"
#include "input/Zmq.h"
#include "input/File.h"
#include "input/Udp.h"


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

static void setup_subchannel_from_ptree(DabSubchannel* subchan,
        const boost::property_tree::ptree &pt,
        std::shared_ptr<dabEnsemble> ensemble,
        const string& subchanuid);

static uint16_t get_announcement_flag_from_ptree(
        boost::property_tree::ptree& pt
        )
{
    uint16_t flags = 0;
    for (size_t flag = 0; flag < 16; flag++) {
        std::string announcement_name(annoucement_flags_names[flag]);
        bool flag_set = pt.get<bool>(announcement_name, false);

        if (flag_set) {
            flags |= (1 << flag);
        }
    }

    return flags;
}

// Parse the linkage section
static void parse_linkage(boost::property_tree::ptree& pt,
        std::shared_ptr<dabEnsemble> ensemble)
{
    using boost::property_tree::ptree;
    using boost::property_tree::ptree_error;

    auto pt_linking = pt.get_child_optional("linking");
    if (pt_linking)
    {
        for (const auto& it : *pt_linking) {
            const string setuid = it.first;
            const ptree pt_set = it.second;

            uint16_t lsn = hexparse(pt_set.get("lsn", "0"));
            if (lsn == 0) {
                etiLog.level(error) << "LSN for linking set " <<
                    setuid << " invalid or missing";
                throw runtime_error("Invalid service linking definition");
            }

            bool hard = pt_set.get("hard", true);
            bool international = pt_set.get("international", false);

            string service_uid = pt_set.get("keyservice", "");
            if (service_uid.empty()) {
                etiLog.level(error) << "Key Service for linking set " <<
                    setuid << " invalid or missing";
                throw runtime_error("Invalid service linking definition");
            }

            auto linkageset = make_shared<LinkageSet>(setuid, lsn, hard, international);
            linkageset->keyservice = service_uid; // TODO check if it exists

            auto pt_list = pt_set.get_child_optional("list");
            if (not pt_list) {
                etiLog.level(error) << "list missing in linking set " <<
                    setuid;
                throw runtime_error("Invalid service linking definition");
            }

            for (const auto& it : *pt_list) {
                const string linkuid = it.first;
                const ptree pt_link = it.second;

                ServiceLink link;

                string link_type = pt_link.get("type", "");
                if (link_type == "dab") link.type = ServiceLinkType::DAB;
                else if (link_type == "fm") link.type = ServiceLinkType::FM;
                else if (link_type == "drm") link.type = ServiceLinkType::DRM;
                else if (link_type == "amss") link.type = ServiceLinkType::AMSS;
                else {
                    etiLog.level(error) << "Invalid type " << link_type <<
                        " for link " << linkuid;
                    throw runtime_error("Invalid service linking definition");
                }

                link.id = hexparse(pt_link.get("id", "0"));
                if (link.id == 0) {
                    etiLog.level(error) << "id for link " <<
                        linkuid << " invalid or missing";
                    throw runtime_error("Invalid service linking definition");
                }

                if (international) {
                    link.ecc = hexparse(pt_link.get("ecc", "0"));
                    if (link.ecc == 0) {
                        etiLog.level(error) << "ecc for link " <<
                            linkuid << " invalid or missing";
                        throw runtime_error("Invalid service linking definition");
                    }
                }
                else {
                    link.ecc = 0;
                }

                linkageset->id_list.push_back(link);
            }
            ensemble->linkagesets.push_back(linkageset);
        }
    }
}

// Parse the FI section
static void parse_freq_info(boost::property_tree::ptree& pt,
        std::shared_ptr<dabEnsemble> ensemble)
{
    using boost::property_tree::ptree;
    using boost::property_tree::ptree_error;

    auto pt_frequency_information = pt.get_child_optional("frequency_information");
    if (pt_frequency_information)
    {
        for (const auto& it_fi : *pt_frequency_information) {
            const string fi_uid = it_fi.first;
            const ptree pt_fi = it_fi.second;

            auto fi = make_shared<FrequencyInformation>();

            fi->uid = fi_uid;

            for (const auto& it_fle : pt_fi) {
                const string fle_uid = it_fle.first;
                const ptree pt_entry = it_fle.second;

                FrequencyListEntry fle;

                fle.uid = fle_uid;

                string rm_str = pt_entry.get("range_modulation", "");
                if (rm_str == "dab") {
                    fle.rm = RangeModulation::dab_ensemble;
                }
                else if (rm_str == "fm") {
                    fle.rm = RangeModulation::fm_with_rds;
                }
                else if (rm_str == "drm") {
                    fle.rm = RangeModulation::drm;
                }
                else if (rm_str == "amss") {
                    fle.rm = RangeModulation::amss;
                }
                else {
                    throw runtime_error("Invalid range_modulation: " + rm_str);
                }

                fle.continuity = pt_entry.get<bool>("continuity");

                try {
                    switch (fle.rm) {
                        case RangeModulation::dab_ensemble:
                            {
                                fle.fi_dab.eid = hexparse(pt_entry.get<string>("eid"));

                                for (const auto& list_it : pt_entry.get_child("frequencies")) {
                                    const string fi_list_uid = list_it.first;
                                    const ptree pt_list_entry = list_it.second;

                                    FrequencyInfoDab::ListElement el;
                                    el.uid = fi_list_uid;
                                    el.frequency = pt_list_entry.get<float>("frequency");

                                    bool signal_mode_1 = pt_list_entry.get("signal_mode_1", false);
                                    bool adjacent = pt_list_entry.get("adjacent", false);

                                    if (adjacent and signal_mode_1) {
                                        el.control_field = FrequencyInfoDab::ControlField_e::adjacent_mode1;
                                    }
                                    else if (adjacent and not signal_mode_1) {
                                        el.control_field = FrequencyInfoDab::ControlField_e::adjacent_no_mode;
                                    }
                                    if (not adjacent and signal_mode_1) {
                                        el.control_field = FrequencyInfoDab::ControlField_e::disjoint_mode1;
                                    }
                                    else if (not adjacent and not signal_mode_1) {
                                        el.control_field = FrequencyInfoDab::ControlField_e::disjoint_no_mode;
                                    }
                                    fle.fi_dab.frequencies.push_back(el);
                                }
                                if (fle.fi_dab.frequencies.size() > 7) {
                                    throw runtime_error("Too many frequency entries in FI " + fle.uid);
                                }
                            } break;
                        case RangeModulation::fm_with_rds:
                            {
                                fle.fi_fm.pi_code = hexparse(pt_entry.get<string>("pi_code"));

                                std::stringstream frequencies_ss;
                                frequencies_ss << pt_entry.get<string>("frequencies");
                                for (std::string freq; std::getline(frequencies_ss, freq, ' '); ) {
                                    fle.fi_fm.frequencies.push_back(std::stof(freq));
                                }
                                if (fle.fi_fm.frequencies.size() > 7) {
                                    throw runtime_error("Too many frequency entries in FI " + fle.uid);
                                }
                            } break;
                        case RangeModulation::drm:
                            {
                                fle.fi_drm.drm_service_id = hexparse(pt_entry.get<string>("drm_id"));

                                std::stringstream frequencies_ss;
                                frequencies_ss << pt_entry.get<string>("frequencies");
                                for (std::string freq; std::getline(frequencies_ss, freq, ' '); ) {
                                    fle.fi_drm.frequencies.push_back(std::stof(freq));
                                }
                                if (fle.fi_drm.frequencies.size() > 7) {
                                    throw runtime_error("Too many frequency entries in FI " + fle.uid);
                                }
                            } break;
                        case RangeModulation::amss:
                            {
                                fle.fi_amss.amss_service_id = hexparse(pt_entry.get<string>("amss_id"));

                                std::stringstream frequencies_ss;
                                frequencies_ss << pt_entry.get<string>("frequencies");
                                for (std::string freq; std::getline(frequencies_ss, freq, ' '); ) {
                                    fle.fi_amss.frequencies.push_back(std::stof(freq));
                                }
                                if (fle.fi_amss.frequencies.size() > 7) {
                                    throw runtime_error("Too many frequency entries in FI " + fle.uid);
                                }
                            } break;
                    } // switch(rm)
                }
                catch (ptree_error &e) {
                    throw runtime_error("invalid configuration for FI " + fle_uid);
                }

                fi->frequency_information.push_back(fle);

            } // for over fle

            ensemble->frequency_information.push_back(fi);

        } // for over fi
    } // if FI present
}

void parse_ptree(
        boost::property_tree::ptree& pt,
        std::shared_ptr<dabEnsemble> ensemble)
{
    using boost::property_tree::ptree;
    using boost::property_tree::ptree_error;
    /******************** READ GENERAL OPTIONS *****************/
    ptree pt_general = pt.get_child("general");

    /* Dab mode logic */
    switch (pt_general.get("dabmode", 1)) {
        case 1:
            ensemble->transmission_mode = TransmissionMode_e::TM_I;
            break;
        case 2:
            ensemble->transmission_mode = TransmissionMode_e::TM_II;
            break;
        case 3:
            ensemble->transmission_mode = TransmissionMode_e::TM_III;
            break;
        case 4:
            ensemble->transmission_mode = TransmissionMode_e::TM_IV;
            break;
        default:
            throw runtime_error("Mode must be between 1-4");
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

    try {
        ptree pt_announcements = pt_ensemble.get_child("announcements");
        for (auto announcement : pt_announcements) {
            string name = announcement.first;
            ptree pt_announcement = announcement.second;

            auto cl = make_shared<AnnouncementCluster>(name);
            cl->cluster_id = hexparse(pt_announcement.get<string>("cluster"));
            cl->flags = get_announcement_flag_from_ptree(
                    pt_announcement.get_child("flags"));
            cl->subchanneluid = pt_announcement.get<string>("subchannel");

            rcs.enrol(cl.get());
            ensemble->clusters.push_back(cl);
        }
    }
    catch (ptree_error& e) {
        etiLog.level(info) << "No announcements defined in ensemble";
        etiLog.level(debug) << "because " << e.what();
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

        for (auto srv : ensemble->services) {
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

        try {
            /* Parse announcements */
            service->ASu = get_announcement_flag_from_ptree(
                    pt_service.get_child("announcements"));

            auto clusterlist = pt_service.get<std::string>("announcements.clusters", "");
            vector<string> clusters_s;
            boost::split(clusters_s,
                    clusterlist,
                    boost::is_any_of(","));

            for (const auto& cluster_s : clusters_s) {
                if (cluster_s == "") {
                    continue;
                }
                try {
                    service->clusters.push_back(hexparse(cluster_s));
                }
                catch (std::logic_error& e) {
                    etiLog.level(warn) << "Cannot parse '" << clusterlist <<
                        "' announcement clusters for service " << serviceuid <<
                        ": " << e.what();
                }
            }

            if (service->ASu != 0 and service->clusters.empty()) {
                etiLog.level(warn) << "Cluster list for service " << serviceuid <<
                    "is empty, but announcements are defined";
            }
        }
        catch (ptree_error& e) {
            service->ASu = 0;
            service->clusters.clear();

            etiLog.level(info) << "No announcements defined in service " <<
                serviceuid;
        }

        try {
            auto oelist = pt_service.get<std::string>("other_ensembles", "");
            vector<string> oe_string_list;
            boost::split(oe_string_list, oelist, boost::is_any_of(","));

            for (const auto& oe_string : oe_string_list) {
                if (oe_string == "") {
                    continue;
                }
                try {
                    service->other_ensembles.push_back(hexparse(oe_string));
                }
                catch (std::logic_error& e) {
                    etiLog.level(warn) << "Cannot parse '" << oelist <<
                        "' announcement clusters for service " << serviceuid <<
                        ": " << e.what();
                }
            }
        }
        catch (ptree_error& e) {
            etiLog.level(info) << "No other_sensmbles information for Service " <<
                serviceuid;
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
    map<string, DabSubchannel*> allsubchans;

    ptree pt_subchans = pt.get_child("subchannels");
    for (ptree::iterator it = pt_subchans.begin(); it != pt_subchans.end(); ++it) {
        string subchanuid = it->first;
        auto subchan = new DabSubchannel(subchanuid);

        ensemble->subchannels.push_back(subchan);

        try {
            setup_subchannel_from_ptree(subchan, it->second, ensemble,
                    subchanuid);
        }
        catch (runtime_error &e) {
            etiLog.log(error,
                    "%s\n", e.what());
            throw;
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

        DabSubchannel* subchannel;
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

        auto component = new DabComponent(componentuid);

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

        if (component->SCIdS == 0 and not component->label.long_label().empty()) {
            etiLog.level(warn) << "Primary component " << component->uid <<
                " has label set. Since V2.1.1 of the specification, only secondary"
                " components are allowed to have labels.";
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

    parse_linkage(pt, ensemble);
    parse_freq_info(pt, ensemble);
}

static Inputs::dab_input_zmq_config_t setup_zmq_input(
        const boost::property_tree::ptree &pt,
        const std::string& subchanuid)
{
    using boost::property_tree::ptree_error;

    Inputs::dab_input_zmq_config_t zmqconfig;

    try {
        zmqconfig.buffer_size = pt.get<int>("zmq-buffer");
    }
    catch (ptree_error &e) {
        stringstream ss;
        ss << "Subchannel " << subchanuid << ": " << "no zmq-buffer defined!";
        throw runtime_error(ss.str());
    }
    try {
        zmqconfig.prebuffering = pt.get<int>("zmq-prebuffering");
    }
    catch (ptree_error &e) {
        stringstream ss;
        ss << "Subchannel " << subchanuid << ": " << "no zmq-prebuffer defined!";
        throw runtime_error(ss.str());
    }

    zmqconfig.curve_encoder_keyfile = pt.get<string>("encoder-key","");
    zmqconfig.curve_secret_keyfile = pt.get<string>("secret-key","");
    zmqconfig.curve_public_keyfile = pt.get<string>("public-key","");

    zmqconfig.enable_encryption = pt.get<int>("encryption", 0);

    return zmqconfig;
}

static void setup_subchannel_from_ptree(DabSubchannel* subchan,
        const boost::property_tree::ptree &pt,
        std::shared_ptr<dabEnsemble> ensemble,
        const string& subchanuid)
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

    /* Both inputfile and inputuri are supported, and are equivalent.
     * inputuri has precedence
     */
    string inputUri = pt.get<string>("inputuri", "");

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

    string proto;
    size_t protopos = inputUri.find("://");
    if (protopos == string::npos) {
        proto = "file";
    }
    else {
        proto = inputUri.substr(0, protopos);
    }

    subchan->inputUri = inputUri;

    dabProtection* protection = &subchan->protection;

    const bool nonblock = pt.get("nonblock", false);
    if (nonblock) {
        etiLog.level(warn) << "The nonblock option is not supported";
    }

    if (type == "dabplus" or type == "audio") {
        subchan->type = subchannel_type_t::Audio;
        subchan->bitrate = 0;

        if (proto == "file") {
            if (type == "audio") {
                subchan->input = make_shared<Inputs::MPEGFile>();
            }
            else if (type == "dabplus") {
                subchan->input = make_shared<Inputs::RawFile>();
            }
            else {
                throw logic_error("Incomplete handling of file input");
            }
        }
        else if (proto == "tcp"  ||
                 proto == "epgm" ||
                 proto == "ipc") {

            auto zmqconfig = setup_zmq_input(pt, subchanuid);

            if (type == "audio") {
                auto inzmq = make_shared<Inputs::ZmqMPEG>(subchanuid, zmqconfig);
                rcs.enrol(inzmq.get());
                subchan->input = inzmq;
            }
            else if (type == "dabplus") {
                auto inzmq = make_shared<Inputs::ZmqAAC>(subchanuid, zmqconfig);
                rcs.enrol(inzmq.get());
                subchan->input = inzmq;
            }

            if (proto == "epgm") {
                etiLog.level(warn) << "Using untested epgm:// zeromq input";
            }
            else if (proto == "ipc") {
                etiLog.level(warn) << "Using untested ipc:// zeromq input";
            }
        }
        else if (proto == "sti-rtp") {
            subchan->input = make_shared<Inputs::Sti_d_Rtp>();
        }
        else {
            stringstream ss;
            ss << "Subchannel with uid " << subchanuid <<
                ": Invalid protocol for " << type << " input (" <<
                proto << ")" << endl;
            throw runtime_error(ss.str());
        }
    }
    else if (type == "data" and proto == "prbs") {
        subchan->input = make_shared<Inputs::Prbs>();
        subchan->type = subchannel_type_t::DataDmb;
        subchan->bitrate = DEFAULT_DATA_BITRATE;
    }
    else if (type == "data" or type == "dmb") {
        if (proto == "udp") {
            subchan->input = make_shared<Inputs::Udp>();
        } else if (proto == "file" or proto == "fifo") {
            subchan->input = make_shared<Inputs::RawFile>();
        } else {
            stringstream ss;
            ss << "Subchannel with uid " << subchanuid <<
                ": Invalid protocol for data input (" <<
                proto << ")" << endl;
            throw runtime_error(ss.str());
        }

        subchan->type = subchannel_type_t::DataDmb;
        subchan->bitrate = DEFAULT_DATA_BITRATE;

        if (type == "dmb") {
            /* The old dmb input took care of interleaving and Reed-Solomon encoding. This
             * code is unported.
             * See dabInputDmbFile.cpp
             */
            etiLog.level(warn) << "uid " << subchanuid << " of type Dmb uses RAW input";
        }
    }
    else if (type == "packet" or type == "enhancedpacket") {
        subchan->type = subchannel_type_t::Packet;
        subchan->bitrate = DEFAULT_PACKET_BITRATE;

        bool enhanced = (type == "enhancedpacket");
        subchan->input = make_shared<Inputs::PacketFile>(enhanced);
    }
    else {
        stringstream ss;
        ss << "Subchannel with uid " << subchanuid << " has unknown type!";
        throw runtime_error(ss.str());
    }
    subchan->startAddress = 0;

    if (type == "audio") {
        protection->form = UEP;
        protection->level = 2;
        protection->uep.tableIndex = 0;
    }
    else {
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

    /* Get id */
    try {
        subchan->id = hexparse(pt.get<std::string>("id"));
    }
    catch (ptree_error &e) {
        for (int i = 0; i < 64; ++i) { // Find first free subchannel
            vector<DabSubchannel*>::iterator subchannel = getSubchannel(ensemble->subchannels, i);
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
}

