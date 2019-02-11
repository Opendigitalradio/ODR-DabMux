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
#include <cstring>
#include <iostream>
#include <memory>
#include <boost/algorithm/string/join.hpp>
#include "DabMux.h"
#include "utils.h"
#include "fig/FIG0structs.h"

using namespace std;

static time_t dab_time_seconds = 0;
static int dab_time_millis = 0;

void update_dab_time()
{
    if (dab_time_seconds == 0) {
        dab_time_seconds = time(nullptr);
    } else {
        dab_time_millis+= 24;
        if (dab_time_millis >= 1000) {
            dab_time_millis -= 1000;
            ++dab_time_seconds;
        }
    }
}

void get_dab_time(time_t *time, uint32_t *millis)
{
    *time = dab_time_seconds;
    *millis = dab_time_millis;
}


uint32_t gregorian2mjd(int year, int month, int day)
{
    //This is the algorithm for the JD, just substract 2400000.5 for MJD
    year += 8000;
    if(month < 3) {
        year--;
        month += 12;
    }
    uint32_t JD =
        (year * 365) + (year / 4) - (year / 100) + (year / 400) - 1200820
        + ((month * 153 + 3) / 5) - 92 + (day - 1);

    return (uint32_t)(JD - 2400000.5); //truncation, loss of data OK!
}

/* We use fprintf here because this doesn't have
 * to go to the log.
 * But all information below must go into the log.
 *
 * Do not use \n in the log, it messes presentation and logging
 * up.
 */

void header_message()
{
    fprintf(stderr,
            "Welcome to %s %s, compiled at %s, %s",
            PACKAGE_NAME,
#if defined(GITVERSION)
            GITVERSION,
#else
            PACKAGE_VERSION,
#endif
            __DATE__, __TIME__);
    fprintf(stderr, "\n\n");
    fprintf(stderr,
            "Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012\n");
    fprintf(stderr,
            "Her Majesty the Queen in Right of Canada\n");
    fprintf(stderr,
            "(Communications Research Centre Canada)\n\n");
    fprintf(stderr,
            "Copyright (C) 2019 Matthias P. Braendli\n");
    fprintf(stderr,
            "LICENCE: GPLv3+\n\n");
    fprintf(stderr,
            "http://opendigitalradio.org\n\n");

    std::cerr << "Input URLs supported:" << std::endl <<
    " prbs" <<
    " udp" <<
    " file" <<
    " zmq" <<
    std::endl;

    std::cerr << "Inputs format supported:" << std::endl <<
    " raw" <<
    " mpeg" <<
    " packet" <<
    " epm" <<
    std::endl;

    std::cerr << "Output URLs supported:" << std::endl <<
#if defined(HAVE_OUTPUT_FILE)
    " file" <<
#endif
#if defined(HAVE_OUTPUT_FIFO)
    " fifo" <<
#endif
#if defined(HAVE_OUTPUT_UDP)
    " udp" <<
#endif
#if defined(HAVE_OUTPUT_TCP)
    " tcp" <<
#endif
#if defined(HAVE_OUTPUT_RAW)
    " raw" <<
#endif
#if defined(HAVE_OUTPUT_SIMUL)
    " simul" <<
#endif
    std::endl << std::endl;

}

void printUsage(char *name, FILE* out)
{
    fprintf(out, "NAME\n");
    fprintf(out, "  %s - A software DAB multiplexer\n", name);
    fprintf(out, "\nSYNOPSYS\n");
    fprintf(out, "    This software requires a configuration file:\n");
    fprintf(out, "  %s configuration.mux\n", name);
    fprintf(out, "    See doc/example.config for an example format for the configuration file\n");
    fprintf(out, "\nDESCRIPTION\n");
    fprintf(out,
            "  %s  is a software multiplexer that generates an ETI stream from\n"
            "  audio and data streams. Because of  its  software  based  architecture,\n"
            "  many  typical DAB services can be generated and multiplexed on a single\n"
            "  PC platform with live or pre-recorded sources.\n"
            "\n"
            "  A DAB multiplex configuration is composed of one ensemble. An  ensemble\n"
            "  is  the entity that receivers tune to and process. An ensemble contains\n"
            "  several services. A service is  the  listener-selectable  output.  Each\n"
            "  service  contains  one mandatory service component which is called pri-\n"
            "  mary component. An audio primary component  define  a  program  service\n"
            "  while  a data primary component define a data service. Service can con-\n"
            "  tain additional components which are called secondary components. Maxi-\n"
            "  mum  total  number  of components is 12 for program services and 11 for\n"
            "  data services. A service component is a link to one subchannel (of Fast\n"
            "  Information  Data  Channel).  A  subchannel  is the physical space used\n"
            "  within the common interleaved frame.\n"
            "\n"
            "   __________________________________________________\n"
            "  |                     Ensemble                     |  ENSEMBLE\n"
            "  |__________________________________________________|\n"
            "          |                 |                 |\n"
            "          |                 |                 |\n"
            "   _______V______    _______V______    _______V______\n"
            "  |   Service 1  |  |   Service 2  |  |   Service 3  |  SERVICES\n"
            "  |______________|  |______________|  |______________|\n"
            "     |        |        |        | |______         |\n"
            "     |        |        |        |        |        |\n"
            "   __V__    __V__    __V__    __V__    __V__    __V__\n"
            "  | SC1 |  | SC2 |  | SC3 |  | SC4 |  | SC5 |  | SC6 |  SERVICE\n"
            "  |_____|  |_____|  |_____|  |_____|  |_____|  |_____|  COMPONENTS\n"
            "     |        |   _____|        |        |    ____|\n"
            "     |        |  |              |        |   |\n"
            "   __V________V__V______________V________V___V_______   COMMON\n"
            "  | SubCh1 | SubCh9 |  ...  | SubCh3 | SubCh60 | ... |  INTERLEAVED\n"
            "  |________|________|_______|________|_________|_____|  FRAME\n"
            "  Figure 1: An example of a DAB multiplex configuration\n",
        name);
}

void printOutputs(const vector<shared_ptr<DabOutput> >& outputs)
{
    int index = 0;

    for (auto output : outputs) {
        etiLog.log(info, "Output      %i", index);
        etiLog.level(info) << "  URI: " <<
            output->get_info();

        ++index;
    }
}

void printServices(const vector<shared_ptr<DabService> >& services)
{
    int index = 0;

    for (auto service : services) {

        etiLog.level(info) << "Service       " << service->get_rc_name();
        etiLog.level(info) << " label:       " <<
                service->label.long_label();
        etiLog.level(info) << " short label: " <<
                service->label.short_label();

        etiLog.log(info, " (0x%x)", service->label.flag());
        etiLog.log(info, " id:            0x%lx (%lu)", service->id,
                service->id);

        etiLog.log(info, " pty:           0x%x (%u) %s",
                service->pty_settings.pty,
                service->pty_settings.pty,
                service->pty_settings.dynamic_no_static ? "Dynamic" : "Static");

        etiLog.log(info, " language:      0x%x (%u)",
                service->language, service->language);
        etiLog.log(info, " announcements: 0x%x",
                service->ASu);

        std::vector<std::string> clusters_s;
        for (const auto& cluster : service->clusters) {
            clusters_s.push_back(std::to_string(cluster));
        }
        etiLog.level(info) << " clusters: " << boost::join(clusters_s, ",");
        ++index;
    }
}

void printComponents(const vec_sp_component& components)
{
    unsigned int index = 0;

    for (const auto component : components) {
        etiLog.level(info) << "Component     " << component->get_rc_name();
        printComponent(component);
        ++index;
    }
}

void printComponent(const shared_ptr<DabComponent>& component)
{
    etiLog.log(info, " service id:             0x%x (%u)",
            component->serviceId, component->serviceId);
    etiLog.log(info, " subchannel id:          0x%x (%u)",
            component->subchId, component->subchId);
    etiLog.level(info) << " label:                  " <<
            component->label.long_label();
    etiLog.level(info) << " short label:            " <<
            component->label.short_label();

    etiLog.log(info, " (0x%x)", component->label.flag());
    etiLog.log(info, " service component type: 0x%x (%u)", component->type,
            component->type);

    if (component->packet.appType != 0xFFFF) {
        etiLog.log(info, " (packet) id:            0x%x (%u)",
                component->packet.id, component->packet.id);
        etiLog.log(info, " (packet) address:       %u",
                component->packet.address);

        etiLog.log(info, " (packet) app type:      %u",
                component->packet.appType);

        etiLog.log(info, " (packet) datagroup:     %u",
                component->packet.datagroup);
    }
    else if (component->audio.uaType != 0xFFFF) {
        stringstream ss;
        ss <<            " (audio) app type:       ";
        switch (component->audio.uaType) {
            case FIG0_13_APPTYPE_SLIDESHOW:
                ss << "MOT Slideshow";
                break;
            case FIG0_13_APPTYPE_WEBSITE:
                ss << "MOT Broadcast Website";
                break;
            case FIG0_13_APPTYPE_TPEG:
                ss << "TPEG";
                break;
            case FIG0_13_APPTYPE_DGPS:
                ss << "DGPS";
                break;
            case FIG0_13_APPTYPE_TMC:
                ss << "TMC";
                break;
            case FIG0_13_APPTYPE_EPG:
                ss << "EPG";
                break;
            case FIG0_13_APPTYPE_DABJAVA:
                ss << "DAB Java";
                break;
            case FIG0_13_APPTYPE_JOURNALINE:
                ss << "Journaline";
                break;
            default:
                ss << "Unknown: " << component->audio.uaType;
                break;
        }

        etiLog.level(info) << ss.str();
    }
    else {
        etiLog.level(info) << " No app type defined";
    }
}

void printSubchannels(const vec_sp_subchannel& subchannels)
{
    int index = 0;

    int total_num_cu = 0;

    for (auto subchannel : subchannels) {
        dabProtection* protection = &subchannel->protection;
        etiLog.level(info) << "Subchannel   " << subchannel->uid;
        etiLog.log(info, " input");
        etiLog.level(info) << "   URI:     " << subchannel->inputUri;
        switch (subchannel->type) {
            case subchannel_type_t::DABAudio:
                etiLog.log(info, " type:       DAbAudio");
                break;
            case subchannel_type_t::DABPlusAudio:
                etiLog.log(info, " type:       DABPlusAudio");
                break;
            case subchannel_type_t::DataDmb:
                etiLog.log(info, " type:       data");
                break;
            case subchannel_type_t::Fidc:
                etiLog.log(info, " type:       fidc");
                break;
            case subchannel_type_t::Packet:
                etiLog.log(info, " type:       packet");
                break;
            default:
                etiLog.log(info, " type:       unknown");
                break;
        }
        etiLog.log(info, " id:         0x%x (%u)",
                subchannel->id, subchannel->id);
        etiLog.log(info, " bitrate:    %i",
                subchannel->bitrate);
        if (protection->form == UEP) {
            etiLog.log(info, " protection: UEP %i", protection->level + 1);
            etiLog.log(info, "  index:     %i",
                    protection->uep.tableIndex);
        }
        else {
            etiLog.log(info, " protection: EEP %i-%c",
                    protection->level + 1,
                    protection->eep.profile == EEP_A ? 'A' : 'B');
            etiLog.log(info, "  option:    %i",
                    protection->eep.GetOption());
            etiLog.log(info, "  level:     %i",
                    subchannel->protection.level);
        }
        etiLog.log(info, " SAD:        %u",
                subchannel->startAddress);
        etiLog.log(info, " size (CU):  %i",
                subchannel->getSizeCu());
        total_num_cu += subchannel->getSizeCu();
        ++index;
    }

    etiLog.log(info, "Total ensemble size (CU):  %i", total_num_cu);
}

static void printLinking(const shared_ptr<dabEnsemble>& ensemble)
{
    etiLog.log(info, " Linkage Sets");
    if (ensemble->linkagesets.empty()) {
        etiLog.level(info) << "  None ";
    }
    for (const auto& ls : ensemble->linkagesets) {

        etiLog.level(info) << "  set " << ls->get_name();
        etiLog.log(info,      "   LSN 0x%04x", ls->lsn);
        etiLog.level(info) << "   active " << (ls->active ? "true" : "false");
        etiLog.level(info) << "   " << (ls->hard ? "hard" : "soft");
        etiLog.level(info) << "   international " << (ls->international ? "true" : "false");
        etiLog.level(info) << "   key service " << ls->keyservice;

        etiLog.level(info) << "   ID list";
        for (const auto& link : ls->id_list) {
            switch (link.type) {
                case ServiceLinkType::DAB:
                    etiLog.level(info) << "    type DAB";
                    break;
                case ServiceLinkType::FM:
                    etiLog.level(info) << "    type FM";
                    break;
                case ServiceLinkType::DRM:
                    etiLog.level(info) << "    type DRM";
                    break;
                case ServiceLinkType::AMSS:
                    etiLog.level(info) << "    type AMSS";
                    break;
            }
            etiLog.log(info,      "    id 0x%04x", link.id);
            if (ls->international) {
                etiLog.log(info,      "    ecc 0x%04x", link.ecc);
            }
        }
    }

    etiLog.log(info, " Services in other ensembles");
    if (ensemble->service_other_ensemble.empty()) {
        etiLog.level(info) << "  None ";
    }

    for (const auto& s_oe : ensemble->service_other_ensemble) {
        int oe = 1;

        for (const auto& local_service : ensemble->services) {
            if (local_service->id == s_oe.service_id) {
                oe = 0;
                break;
            }
        }

        etiLog.log(info, "  Service 0x%lx", s_oe.service_id);
        for (const auto oe : s_oe.other_ensembles) {
            etiLog.log(info, "   Available in ensemble 0x%04x", oe);
        }
        etiLog.log(info, "   OE=%d", oe);
    }
}

static void printFrequencyInformation(const shared_ptr<dabEnsemble>& ensemble)
{
    etiLog.log(info, " Frequency Information");
    if (ensemble->frequency_information.empty()) {
        etiLog.level(info) << "  None ";
    }
    for (const auto& fi : ensemble->frequency_information) {
        etiLog.level(info) << "  FI " << fi.uid;
        etiLog.level(info) << "   OE=" << (fi.other_ensemble ? 1 : 0);
        switch (fi.rm) {
            case RangeModulation::dab_ensemble:
                etiLog.level(info) << "   RM: DAB";
                etiLog.log(info,      "   EId 0x%04x", fi.fi_dab.eid);
                break;
            case RangeModulation::drm:
                etiLog.level(info) << "   RM: DRM";
                etiLog.log(info,      "   DRM Id 0x%06x", fi.fi_drm.drm_service_id);
                break;
            case RangeModulation::fm_with_rds:
                etiLog.level(info) << "   RM: FM (with RDS)";
                etiLog.log(info,      "   PI-Code 0x%04x", fi.fi_fm.pi_code);
                break;
            case RangeModulation::amss:
                etiLog.level(info) << "   RM: AMSS";
                etiLog.log(info,      "   AMSS Id 0x%06x", fi.fi_amss.amss_service_id);
                break;
        }
        etiLog.level(info) << "   continuity " << (fi.continuity ? "true" : "false");

        switch (fi.rm) {
            case RangeModulation::dab_ensemble:
                for (const auto& f : fi.fi_dab.frequencies) {
                    stringstream ss;
                    ss << "      " << f.uid << " ";
                    switch (f.control_field) {
                        case FrequencyInfoDab::ControlField_e::adjacent_no_mode:
                            ss << "Adjacent, w/o mode indication, ";
                            break;
                        case FrequencyInfoDab::ControlField_e::adjacent_mode1:
                            ss << "Adjacent, mode I, ";
                            break;
                        case FrequencyInfoDab::ControlField_e::disjoint_no_mode:
                            ss << "Disjoint, w/o mode indication, ";
                            break;
                        case FrequencyInfoDab::ControlField_e::disjoint_mode1:
                            ss << "Disjoint, mode I, ";
                            break;
                    }
                    ss << f.frequency;
                    etiLog.level(info) << ss.str();
                }
                break;
            case RangeModulation::drm:
                etiLog.log(info, "    ID 0x%02x", fi.fi_drm.drm_service_id);
                for (const auto& f : fi.fi_drm.frequencies) {
                    etiLog.level(info) << "        " << f;
                }
                break;
            case RangeModulation::fm_with_rds:
                for (const auto& f : fi.fi_fm.frequencies) {
                    etiLog.level(info) << "        " << f;
                }
                break;
            case RangeModulation::amss:
                etiLog.log(info, "    ID 0x%02x", fi.fi_amss.amss_service_id);
                for (const auto& f : fi.fi_amss.frequencies) {
                    etiLog.level(info) << "        " << f;
                }
                break;
        }
    }
}

void printEnsemble(const shared_ptr<dabEnsemble>& ensemble)
{
    etiLog.log(info, "Ensemble");
    etiLog.log(info, " id:          0x%lx (%lu)", ensemble->id, ensemble->id);
    etiLog.log(info, " ecc:         0x%x (%u)", ensemble->ecc, ensemble->ecc);
    etiLog.level(info) << " label:       " <<
            ensemble->label.long_label();
    etiLog.level(info) << " short label: " <<
            ensemble->label.short_label();

    etiLog.log(info, " (0x%x)", ensemble->label.flag());
    switch (ensemble->transmission_mode) {
        case TransmissionMode_e::TM_I:
            etiLog.log(info, " mode:        TM I");
            break;
        case TransmissionMode_e::TM_II:
            etiLog.log(info, " mode:        TM II");
            break;
        case TransmissionMode_e::TM_III:
            etiLog.log(info, " mode:        TM III");
            break;
        case TransmissionMode_e::TM_IV:
            etiLog.log(info, " mode:        TM IV");
            break;
    }

    if (ensemble->lto_auto) {
        time_t now = time(nullptr);
        struct tm* ltime = localtime(&now);
        time_t now2 = timegm(ltime);
        etiLog.log(info, " lto:         %2.1f hours", 0.5 * (now2 - now) / 1800);
    }
    else {
        etiLog.log(info, " lto:         %2.1f hours", 0.5 * ensemble->lto);
    }
    etiLog.log(info, " intl. table. %d", ensemble->international_table);

    if (ensemble->clusters.empty()) {
        etiLog.level(info) << " No announcement clusters defined";
    }
    else {
        etiLog.level(info) << " Announcement clusters:";
        for (const auto& cluster : ensemble->clusters) {
            etiLog.level(info) << "  " << cluster->tostring();
        }
    }

    printLinking(ensemble);
    printFrequencyInformation(ensemble);
}

long hexparse(const std::string& input)
{
    long value = 0;
    errno = 0;

    char* endptr = nullptr;

    const bool is_hex = (input.find("0x") == 0);

    // Do not use strtol's base=0 because
    // we do not want to accept octal.
    const int base = is_hex ? 16 : 10;

    const char* const startptr = is_hex ?
        input.c_str() + 2 :
        input.c_str();

    value = strtol(input.c_str(), &endptr, base);

    if ((value == LONG_MIN or value == LONG_MAX) and errno == ERANGE) {
        throw out_of_range("hexparse: value out of range");
    }
    else if (value == 0 and errno != 0) {
        stringstream ss;
        ss << "hexparse: " << strerror(errno);
        throw invalid_argument(ss.str());
    }

    if (startptr == endptr) {
        throw out_of_range("hexparse: no value found");
    }

    if (*endptr != '\0') {
        throw out_of_range("hexparse: superfluous characters after value found: '" + input + "'");
    }

    return value;
}

