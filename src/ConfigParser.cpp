/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2022
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

#include "utils.h"
#include "input/Edi.h"
#include "input/Prbs.h"
#include "input/Zmq.h"
#include "input/File.h"
#include "input/Udp.h"
#include "fig/FIG0structs.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <exception>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace std;
using boost::property_tree::ptree;
using boost::property_tree::ptree_error;

constexpr uint16_t DEFAULT_DATA_BITRATE = 384;
constexpr uint16_t DEFAULT_PACKET_BITRATE = 32;

constexpr uint32_t DEFAULT_SERVICE_ID = 50;


static void setup_subchannel_from_ptree(shared_ptr<DabSubchannel>& subchan,
        const ptree &pt,
        std::shared_ptr<dabEnsemble> ensemble,
        const string& subchanuid);

static uint16_t get_announcement_flag_from_ptree(ptree& pt)
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

static void parse_fig2_label(ptree& pt, DabLabel& label) {
    label.setFIG2Label(pt.get<string>("fig2_label", ""));

    uint16_t character_field = hexparse(pt.get("fig2_label_character_flag", "0"));
    if (character_field) {
        label.setFIG2CharacterField(character_field);
    }

    auto pt_tc = pt.get_child_optional("fig2_label_text_control");
    if (pt_tc) {
        FIG2TextControl tc;
        tc.bidi_flag = pt_tc->get<bool>("bidi", tc.bidi_flag);
        auto base_direction = pt_tc->get<string>("base_direction", "LTR");

        tc.base_direction_is_rtl = (base_direction == "RTL");

        tc.contextual_flag = pt_tc->get<bool>("contextual", tc.contextual_flag);
        tc.combining_flag = pt_tc->get<bool>("combining", tc.combining_flag);

        label.setFIG2TextControl(tc);
    }
}

// Parse the linkage section
static void parse_linkage(ptree& pt,
        std::shared_ptr<dabEnsemble> ensemble)
{
    auto pt_linking = pt.get_child_optional("linking");
    if (pt_linking) {
        for (const auto& it : *pt_linking) {
            const string setuid = it.first;
            const ptree pt_set = it.second;

            uint16_t lsn = hexparse(pt_set.get("lsn", "0"));
            if (lsn == 0 or lsn > 0xFFF) {
                etiLog.level(error) << "LSN for linking set " <<
                    setuid << " invalid or missing";
                throw runtime_error("Invalid service linking definition");
            }

            bool active = pt_set.get("active", true);
            bool hard = pt_set.get("hard", true);
            bool international = pt_set.get("international", false);
            string service_uid = pt_set.get("keyservice", "");

            auto linkageset = make_shared<LinkageSet>(setuid, lsn, active, hard, international);
            linkageset->keyservice = service_uid; // TODO check if it exists

            auto pt_list = pt_set.get_child_optional("list");

            if (service_uid.empty()) {
                if (pt_list) {
                    etiLog.level(error) << "list defined but no keyservice in linking set " <<
                        setuid;
                    throw runtime_error("Invalid service linking definition");
                }
            }
            else {
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
            }
            ensemble->linkagesets.push_back(linkageset);
        }
    }
}

// Parse the FI section
static void parse_freq_info(ptree& pt, std::shared_ptr<dabEnsemble> ensemble)
{
    auto pt_frequency_information = pt.get_child_optional("frequency_information");
    if (pt_frequency_information)
    {
        for (const auto& it_fi : *pt_frequency_information) {
            const string fi_uid = it_fi.first;
            const ptree pt_fi = it_fi.second;

            FrequencyInformation fi;

            fi.uid = fi_uid;

            fi.other_ensemble = pt_fi.get("oe", false);
            string rm_str = pt_fi.get("range_modulation", "");
            if (rm_str == "dab") {
                fi.rm = RangeModulation::dab_ensemble;
            }
            else if (rm_str == "fm") {
                fi.rm = RangeModulation::fm_with_rds;
            }
            else if (rm_str == "drm") {
                fi.rm = RangeModulation::drm;
            }
            else if (rm_str == "amss") {
                fi.rm = RangeModulation::amss;
            }
            else if (rm_str == "") {
                throw runtime_error("Missing range_modulation in FI " + fi_uid);
            }
            else {
                throw runtime_error("Invalid range_modulation '" + rm_str +
                        "' in FI " + fi_uid);
            }

            fi.continuity = pt_fi.get<bool>("continuity");

            try {
                switch (fi.rm) {
                    case RangeModulation::dab_ensemble:
                        {
                            fi.fi_dab.eid = hexparse(pt_fi.get<string>("eid"));

                            for (const auto& list_it : pt_fi.get_child("frequencies")) {
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
                                fi.fi_dab.frequencies.push_back(el);
                            }
                            if (fi.fi_dab.frequencies.empty()) {
                                throw runtime_error("Empty frequency list in FI " + fi.uid);
                            }
                        } break;
                    case RangeModulation::fm_with_rds:
                        {
                            fi.fi_fm.pi_code = hexparse(pt_fi.get<string>("pi_code"));

                            std::stringstream frequencies_ss;
                            frequencies_ss << pt_fi.get<string>("frequencies");
                            for (std::string freq; std::getline(frequencies_ss, freq, ' '); ) {
                                fi.fi_fm.frequencies.push_back(std::stof(freq));
                            }
                            if (fi.fi_fm.frequencies.empty()) {
                                throw runtime_error("Empty frequency list in FI " + fi.uid);
                            }
                        } break;
                    case RangeModulation::drm:
                        {
                            fi.fi_drm.drm_service_id = hexparse(pt_fi.get<string>("drm_id"));

                            std::stringstream frequencies_ss;
                            frequencies_ss << pt_fi.get<string>("frequencies");
                            for (std::string freq; std::getline(frequencies_ss, freq, ' '); ) {
                                fi.fi_drm.frequencies.push_back(std::stof(freq));
                            }
                            if (fi.fi_drm.frequencies.empty()) {
                                throw runtime_error("Empty frequency list in FI " + fi.uid);
                            }
                        } break;
                    case RangeModulation::amss:
                        {
                            fi.fi_amss.amss_service_id = hexparse(pt_fi.get<string>("amss_id"));

                            std::stringstream frequencies_ss;
                            frequencies_ss << pt_fi.get<string>("frequencies");
                            for (std::string freq; std::getline(frequencies_ss, freq, ' '); ) {
                                fi.fi_amss.frequencies.push_back(std::stof(freq));
                            }
                            if (fi.fi_amss.frequencies.empty()) {
                                throw runtime_error("Empty frequency list in FI " + fi.uid);
                            }
                        } break;
                } // switch(rm)
            }
            catch (const ptree_error &e) {
                throw runtime_error("invalid configuration for FI " + fi_uid);
            }

            ensemble->frequency_information.emplace_back(move(fi));
        } // for over fi

        /* We sort all FI to have the OE=0 first and the OE=1 afterwards, to
         * avoid having to send FIG0 headers every time it switches.  */
        std::sort(
                ensemble->frequency_information.begin(),
                ensemble->frequency_information.end(),
                [](const FrequencyInformation& first,
                   const FrequencyInformation& second) {
                    const int oe_first = first.other_ensemble ? 1 : 0;
                    const int oe_second = second.other_ensemble ? 1 : 0;
                    return oe_first < oe_second;
                } );
    } // if FI present
}

static void parse_other_service_linking(ptree& pt,
        std::shared_ptr<dabEnsemble> ensemble)
{
    auto pt_other_services = pt.get_child_optional("other-services");
    if (pt_other_services)
    {
        for (const auto& it_service : *pt_other_services) {
            const string srv_uid = it_service.first;
            const ptree pt_srv = it_service.second;

            ServiceOtherEnsembleInfo info;
            try {
                info.service_id = hexparse(pt_srv.get<string>("id"));

                auto oelist = pt_srv.get<std::string>("other_ensembles", "");

                if (not oelist.empty()) {
                    vector<string> oe_string_list;
                    boost::split(oe_string_list, oelist, boost::is_any_of(","));

                    for (const auto& oe_string : oe_string_list) {
                        if (oe_string == "") {
                            continue;
                        }
                        try {
                            info.other_ensembles.push_back(hexparse(oe_string));
                        }
                        catch (const std::exception& e) {
                            etiLog.level(warn) << "Cannot parse '" << oelist <<
                                "' other_ensembles for service " << srv_uid <<
                                ": " << e.what();
                        }
                    }

                    ensemble->service_other_ensemble.push_back(move(info));
                }
            }
            catch (const std::exception &e) {
                etiLog.level(warn) <<
                    "Cannot parse other_ensembles for service " << srv_uid <<
                    ": " << e.what();
            }

        } // for over services
    } // if other-services present
}

static void parse_general(ptree& pt,
        std::shared_ptr<dabEnsemble> ensemble)
{
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
    if (ensemble->id == 0) {
        etiLog.level(warn) << "Ensemble ID is 0!";
    }

    /* Extended Country Code */
    ensemble->ecc = hexparse(pt_ensemble.get("ecc", "0"));
    if (ensemble->ecc == 0) {
        etiLog.level(warn) << "ECC is 0!";
    }

    ensemble->international_table = pt_ensemble.get("international-table", ensemble->international_table);

    bool reconfig_counter_is_hash = false;
    try {
        if (pt_ensemble.get<string>("reconfig-counter") == "hash") {
            reconfig_counter_is_hash = true;
        }
    }
    catch (const ptree_error &e) {
    }

    if (reconfig_counter_is_hash) {
        ensemble->reconfig_counter = -2;
    }
    else {
        ensemble->reconfig_counter = pt_ensemble.get("reconfig-counter", ensemble->reconfig_counter);
    }

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
    string ensemble_label = pt_ensemble.get<string>("label", "");
    string ensemble_short_label(ensemble_label);
    try {
        ensemble_short_label = pt_ensemble.get<string>("shortlabel");
        success = ensemble->label.setLabel(ensemble_label, ensemble_short_label);
    }
    catch (const ptree_error &e) {
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

    parse_fig2_label(pt_ensemble, ensemble->label);

    try {
        ptree pt_announcements = pt_ensemble.get_child("announcements");
        for (auto announcement : pt_announcements) {
            string name = announcement.first;
            ptree pt_announcement = announcement.second;

            auto cl = make_shared<AnnouncementCluster>(name);
            cl->cluster_id = hexparse(pt_announcement.get<string>("cluster"));
            if (cl->cluster_id == 0) {
                throw runtime_error("Announcement cluster id " +
                        to_string(cl->cluster_id) + " is not allowed");
            }
            if (cl->cluster_id == 255) {
                etiLog.level(debug) <<
                    "Alarm flag for FIG 0/0 is set 1, because announcement group with cluster id oxFF is found.";
                ensemble->alarm_flag = 1;
            }
            cl->flags = get_announcement_flag_from_ptree(
                    pt_announcement.get_child("flags"));
            cl->subchanneluid = pt_announcement.get<string>("subchannel");

            rcs.enrol(cl.get());
            ensemble->clusters.push_back(cl);
        }
    }
    catch (const ptree_error& e) {
        etiLog.level(info) <<
            "No announcements defined in ensemble because " << e.what();
    }
}

void parse_ptree(
        boost::property_tree::ptree& pt,
        std::shared_ptr<dabEnsemble> ensemble)
{
    parse_general(pt, ensemble);

    /******************** READ SERVICES PARAMETERS *************/
    map<string, shared_ptr<DabService> > allservices;

    /* For each service, we keep a separate SCIdS counter */
    map<shared_ptr<DabService>, int> SCIdS_per_service;

    ptree pt_services = pt.get_child("services");
    for (ptree::iterator it = pt_services.begin();
            it != pt_services.end(); ++it) {
        string serviceuid = it->first;
        ptree pt_service = it->second;

        stringstream def_serviceid;
        def_serviceid << DEFAULT_SERVICE_ID + ensemble->services.size();
        uint32_t new_service_id = hexparse(pt_service.get("id", def_serviceid.str()));

        // Ensure that both UID and service ID are unique
        for (auto srv : ensemble->services) {
            if (srv->uid == serviceuid) {
                throw runtime_error("Duplicate service uid " + serviceuid);
            }

            if (srv->id == new_service_id) {
                throw runtime_error("Duplicate service id " + to_string(new_service_id));
            }
        }

        auto service = make_shared<DabService>(serviceuid);
        ensemble->services.push_back(service);

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
                    auto cluster_id = hexparse(cluster_s);
                    if (cluster_id == 255) {
                        etiLog.level(warn) << "Announcement cluster id " +
                                to_string(cluster_id) + " is not allowed and is ignored in FIG 0/18.";
                        continue;
                    }
                    service->clusters.push_back(cluster_id);
                }
                catch (const std::exception& e) {
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
        catch (const ptree_error& e) {
            service->ASu = 0;
            service->clusters.clear();

            etiLog.level(info) << "No announcements defined in service " <<
                serviceuid;
        }

        int success = -5;

        string servicelabel = pt_service.get<string>("label", "");
        string serviceshortlabel(servicelabel);
        try {
            serviceshortlabel = pt_service.get<string>("shortlabel");
            success = service->label.setLabel(servicelabel, serviceshortlabel);
        }
        catch (const ptree_error &e) {
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

        parse_fig2_label(pt_service, service->label);

        service->id = new_service_id;
        service->ecc = hexparse(pt_service.get("ecc", "0"));
        service->pty_settings.pty = hexparse(pt_service.get("pty", "0"));
        // Default to dynamic for backward compatibility
        const string dynamic_no_static_str = pt_service.get("pty-sd", "dynamic");
        if (dynamic_no_static_str == "dynamic") {
            service->pty_settings.dynamic_no_static = true;
        }
        else if (dynamic_no_static_str == "static") {
            service->pty_settings.dynamic_no_static = false;
        }
        else {
            throw runtime_error("pty-sd setting for service " +
                    serviceuid + " is invalid");
        }
        service->language = hexparse(pt_service.get("language", "0"));

        auto oelist = pt_service.get<std::string>("other_ensembles", "");
        if (not oelist.empty()) {
            throw runtime_error(
                "You are using the deprecated other_ensembles inside "
                "'services' specification. Please see doc/servicelinking.mux "
                "for the new syntax.");
        }

        allservices[serviceuid] = service;

        // Set the service's SCIds to zero
        SCIdS_per_service[service] = 0;

        if (allservices.count(serviceuid) != 1) {
            throw logic_error("Assertion error: duplicated service with uid " + serviceuid);
        }
    }


    /******************** READ SUBCHAN PARAMETERS **************/
    map<string, shared_ptr<DabSubchannel> > allsubchans;

    ptree pt_subchans = pt.get_child("subchannels");
    for (ptree::iterator it = pt_subchans.begin(); it != pt_subchans.end(); ++it) {
        string subchanuid = it->first;
        auto subchan = make_shared<DabSubchannel>(subchanuid);

        ensemble->subchannels.push_back(subchan);

        try {
            setup_subchannel_from_ptree(subchan, it->second, ensemble,
                    subchanuid);
        }
        catch (const runtime_error &e) {
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
    map<string, shared_ptr<DabComponent> > allcomponents;
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
        catch (const ptree_error &e) {
            stringstream ss;
            ss << "Component with uid " << componentuid << " is missing service definition!";
            throw runtime_error(ss.str());
        }

        shared_ptr<DabSubchannel> subchannel;
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
        catch (const ptree_error &e) {
            stringstream ss;
            ss << "Component with uid " << componentuid << " is missing subchannel definition!";
            throw runtime_error(ss.str());
        }

        uint8_t component_type = hexparse(pt_comp.get("type", "0"));

        auto component = make_shared<DabComponent>(componentuid);

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
        catch (const ptree_error &e) {
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

        parse_fig2_label(pt_comp, component->label);

        if (component->SCIdS == 0 and not component->label.long_label().empty()) {
            etiLog.level(warn) << "Primary component " << component->uid <<
                " has label set. Since V2.1.1 of the specification, only secondary"
                " components are allowed to have labels.";
        }

        auto pt_ua = pt_comp.get_child_optional("user-applications");
        if (pt_ua) {
            for (const auto& ua_entry : *pt_ua) {
                const string ua_key = ua_entry.first;
                const string ua_value = ua_entry.second.data();

                if (ua_key != "userapp") {
                    etiLog.level(error) << "user-applications should only contain 'userapp' keys";
                    throw runtime_error("component user-applications definition error");
                }

                userApplication ua;

                // Values from TS 101 756 Table 16

                if (ua_value == "slideshow") {
                    ua.uaType = FIG0_13_APPTYPE_SLIDESHOW;

                    // This was previously hardcoded in FIG0/13 and means "MOT, start of X-PAD data group"
                    ua.xpadAppType = 12;
                }
                else if (ua_value == "spi") {
                    ua.uaType = FIG0_13_APPTYPE_SPI;
                    ua.xpadAppType = 16;
                }
                else if (ua_value == "website") {
                    ua.uaType = FIG0_13_APPTYPE_WEBSITE;
                    ua.xpadAppType = 12;
                }
                else if (ua_value == "journaline") {
                    ua.uaType = FIG0_13_APPTYPE_JOURNALINE;
                    ua.xpadAppType = 12;
                }

                if (component->isPacketComponent(ensemble->subchannels)) {
                    component->packet.uaTypes.push_back(ua);
                }
                else {
                    component->audio.uaTypes.push_back(ua);
                }
            }
        }
        else {
            // Setting only figtype is the old format which allows the definition of a single
            // user application type only.
            int figType = hexparse(pt_comp.get("figtype", "-1"));
            if (figType != -1) {

                etiLog.level(warn) << "The figtype setting is deprecated in favour of user-applications. Please see example configurations.";

                if (figType >= (1<<12)) {
                    stringstream ss;
                    ss << "Component with uid " << componentuid <<
                        ": figtype '" << figType << "' is too large !";
                    throw runtime_error(ss.str());
                }

                userApplication ua;
                ua.uaType = figType;

                // This was previously hardcoded in FIG0/13 and means "MOT, start of X-PAD data group"
                ua.xpadAppType = 12;

                if (component->isPacketComponent(ensemble->subchannels)) {
                    component->packet.uaTypes.push_back(ua);
                }
                else {
                    component->audio.uaTypes.push_back(ua);
                }
            }
        }

        if (component->isPacketComponent(ensemble->subchannels)) {
            int packet_address = hexparse(pt_comp.get("address", "-1"));
            if (packet_address != -1) {
                if (! component->isPacketComponent(ensemble->subchannels)) {
                    stringstream ss;
                    ss << "Component with uid " << componentuid <<
                        " is not packet, cannot have address defined !";
                    throw runtime_error(ss.str());
                }

                component->packet.address = packet_address;
            }

            int packet_datagroup = pt_comp.get("datagroup", false);
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
    parse_other_service_linking(pt, ensemble);
}

static Inputs::dab_input_zmq_config_t setup_zmq_input(
        const ptree &pt,
        const std::string& subchanuid)
{
    Inputs::dab_input_zmq_config_t zmqconfig;

    try {
        zmqconfig.buffer_size = pt.get<int>("zmq-buffer");
    }
    catch (const ptree_error &e) {
        stringstream ss;
        ss << "Subchannel " << subchanuid << ": " << "no zmq-buffer defined!";
        throw runtime_error(ss.str());
    }
    try {
        zmqconfig.prebuffering = pt.get<int>("zmq-prebuffering");
    }
    catch (const ptree_error &e) {
        stringstream ss;
        ss << "Subchannel " << subchanuid << ": " << "no zmq-prebuffer defined!";
        throw runtime_error(ss.str());
    }

    zmqconfig.curve_encoder_keyfile = pt.get<string>("encoder-key","");
    zmqconfig.curve_secret_keyfile = pt.get<string>("secret-key","");
    zmqconfig.curve_public_keyfile = pt.get<string>("public-key","");

    zmqconfig.enable_encryption = pt.get<bool>("encryption", false);

    return zmqconfig;
}

static void setup_subchannel_from_ptree(shared_ptr<DabSubchannel>& subchan,
        const ptree &pt,
        std::shared_ptr<dabEnsemble> ensemble,
        const string& subchanuid)
{
    string type;
    /* Read type first */
    try {
        type = pt.get<string>("type");
    }
    catch (const ptree_error &e) {
        throw runtime_error("Subchannel with uid " + subchanuid + " has no type defined!");
    }

    /* Up to v2.3.1, both inputfile and inputuri are supported, and are
     * equivalent.  inputuri has precedence.
     *
     * After that, either inputfile or the (inputproto, inputuri) pair must be given, but not both.
     */
    string inputUri = pt.get<string>("inputuri", "");
    string proto = pt.get<string>("inputproto", "");

    if (inputUri.empty() and proto.empty()) {
        try {
            /* Old approach, derives proto from scheme used in the URL.
             * This makes it impossible to distinguish between ZMQ tcp:// and
             * EDI tcp://
             */
            inputUri = pt.get<string>("inputfile");
            size_t protopos = inputUri.find("://");

            if (protopos == string::npos) {
                proto = "file";
            }
            else {
                proto = inputUri.substr(0, protopos);

                if (proto == "tcp" or proto == "epgm" or proto == "ipc") {
                    proto = "zmq";
                }
                else if (proto == "sti-rtp") {
                    proto = "sti";
                }
            }
        }
        catch (const ptree_error &e) {
            throw runtime_error("Subchannel with uid " + subchanuid + " has no input defined!");
        }
    }
    else if (inputUri.empty() or proto.empty()) {
        throw runtime_error("Must define both inputuri and inputproto for uid " + subchanuid);
    }

    subchan->inputUri = inputUri;

    if (type == "dabplus" or type == "audio") {
        if(type == "dabplus") {
            subchan->type = subchannel_type_t::DABPlusAudio;
        } else {
            subchan->type = subchannel_type_t::DABAudio;
        }

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
        else if (proto == "zmq") {
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
        }
        else if (proto == "edi") {
            Inputs::dab_input_edi_config_t config;
            config.buffer_size = pt.get("buffer", config.buffer_size);
            config.prebuffering = pt.get("prebuffering", config.prebuffering);
            auto inedi = make_shared<Inputs::Edi>(subchanuid, config);
            rcs.enrol(inedi.get());
            subchan->input = inedi;
        }
        else if (proto == "stp") {
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

        subchan->packet_enhanced = (type == "enhancedpacket");
        subchan->input = make_shared<Inputs::PacketFile>(subchan->packet_enhanced);
    }
    else {
        stringstream ss;
        ss << "Subchannel with uid " << subchanuid << " has unknown type!";
        throw runtime_error(ss.str());
    }

    if (pt.get("nonblock", false)) {
        if (auto filein = dynamic_pointer_cast<Inputs::FileBase>(subchan->input)) {
            filein->setNonblocking(true);
        }
        else {
            etiLog.level(warn) << "The nonblock option is not supported";
        }
    }

    if (pt.get("load_entire_file", false)) {
        if (auto filein = dynamic_pointer_cast<Inputs::FileBase>(subchan->input)) {
            filein->setLoadEntireFile(true);
        }
        else {
            etiLog.level(warn) << "The load_entire_file option is not supported";
        }
    }

    const string bufferManagement = pt.get("buffer-management", "prebuffering");
    if (bufferManagement == "prebuffering") {
        subchan->input->setBufferManagement(Inputs::BufferManagement::Prebuffering);
    }
    else if (bufferManagement == "timestamped") {
        subchan->input->setBufferManagement(Inputs::BufferManagement::Timestamped);
    }
    else {
        throw runtime_error("Subchannel with uid " + subchanuid + " has invalid buffer-management !");
    }

    const int32_t tist_delay = pt.get("tist-delay", 0);
    subchan->input->setTistDelay(chrono::milliseconds(tist_delay));

    subchan->startAddress = 0;

    dabProtection* protection = &subchan->protection;
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
    catch (const ptree_error &e) {
        throw runtime_error("Error, no bitrate defined for subchannel " + subchanuid);
    }

    /* Get id */
    try {
        subchan->id = hexparse(pt.get<std::string>("id"));
        if (subchan->id >= 64) {
            throw runtime_error("Invalid subchannel id " + to_string(subchan->id));
        }
    }
    catch (const ptree_error &e) {
        for (int i = 0; i < 64; ++i) { // Find first free subchannel
            auto subchannel = getSubchannel(ensemble->subchannels, i);
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
    else if (profile == "") {
        /* take defaults */
    }
    else {
        throw runtime_error("Invalid protection-profile: " + profile);
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
    catch (const ptree_error &e) {
        stringstream ss;
        ss << "Subchannel with uid " << subchanuid <<
            ": protection level undefined!";
        throw runtime_error(ss.str());
    }
}

