/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2016
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
#include <cstring>
#include <iostream>
#include <memory>
#include <boost/algorithm/string/join.hpp>
#include "DabMux.h"
#include "utils.h"

using namespace std;

static time_t dab_time_seconds = 0;
static int dab_time_millis = 0;

void update_dab_time()
{
    if (dab_time_seconds == 0) {
        dab_time_seconds = time(NULL);
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
            "(Communications Research Centre Canada) All rights reserved.\n\n");
    fprintf(stderr,
            "Copyright (C) 2013, 2014, 2015 Matthias P. Braendli\n");
    fprintf(stderr,
            "http://opendigitalradio.org\n\n");

    std::cerr << "Input URLs supported:" << std::endl <<
#if defined(HAVE_INPUT_PRBS)
    " prbs" <<
#endif
#if defined(HAVE_INPUT_TEST)
    " test" <<
#endif
#if defined(HAVE_INPUT_SLIP)
    " slip" <<
#endif
#if defined(HAVE_INPUT_UDP)
    " udp" <<
#endif
#if defined(HAVE_INPUT_FIFO)
    " fifo" <<
#endif
#if defined(HAVE_INPUT_FILE)
    " file" <<
#endif
#if defined(HAVE_INPUT_ZEROMQ)
    " zmq" <<
#endif
    std::endl;

    std::cerr << "Inputs format supported:" << std::endl <<
#if defined(HAVE_FORMAT_RAW)
    " raw" <<
#endif
#if defined(HAVE_FORMAT_BRIDGE)
    " bridge" <<
#endif
#if defined(HAVE_FORMAT_MPEG)
    " mpeg" <<
#endif
#if defined(HAVE_FORMAT_PACKET)
    " packet" <<
#endif
#if defined(HAVE_FORMAT_DMB)
    " dmb" <<
#endif
#if defined(HAVE_FORMAT_EPM)
    " epm" <<
#endif
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

#if ENABLE_CMDLINE_OPTIONS
void printUsage(char *name, FILE* out)
{
    fprintf(out, "NAME\n");
    fprintf(out, "  %s - A software DAB multiplexer\n", name);
    fprintf(out, "\nSYNOPSYS\n");
    fprintf(out, "    You can use either a configuration file, or the command line arguments\n");
    fprintf(out, "    With external configuration file:\n");
    fprintf(out, "  %s [-e] configuration.mux\n", name);
    fprintf(out, "    See doc/example.config for an example format for the configuration file\n");
    fprintf(out, "    With command line arguments:\n");
    fprintf(out, "  %s"
            " [ensemble]"
            " [subchannel1 subchannel2 ...]"
            " [service1 component1 [component2 ...] service2 ...]"
            " [output1 ...]"
            " [-h]"
            " [-m mode]"
            " [-n nbFrames]"
            " [-o]"
            " [-s]"
            " [-V]"
            " [-z]"
            "\n",
            name);
    fprintf(out, "\n  Where ensemble ="
            " [-i ensembleId]"
            " [-L label]"
            " [-l sLabel]"
            "\n");
    fprintf(out, "\n  Where subchannel ="
            " -(A | B | D | E | F | M | P | T) inputName"
            " [-b bitrate]"
            " [-i subchannelId]"
            " [-k]"
            " [-p protection]"
            "\n");
    fprintf(out, "\n  Where service ="
            " -S"
            " [-g language]"
            " [-i serviceId]"
            " [-L label]"
            " [-l sLabel]"
            " [-y PTy]"
            "\n");
    fprintf(out, "\n  Where component ="
            " -C"
            " [-a address]"
            " [-d]"
            " [-f figType]"
            " [-i subchannelId]"
            " [-L label]"
            " [-l sLabel]"
            " [-t type]"
            "\n");
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

    fprintf(out, "\nGENERAL OPTIONS\n");
    fprintf(out, "  -h                   : get this help\n");
    fprintf(out, "  -m                   : DAB mode (default: 2)\n");
    fprintf(out, "  -n nbFrames          : number of frames to produce\n");
    fprintf(out, "  -o                   : enable logging to syslog\n");
    fprintf(out, "  -r                   : throttle the output rate to one ETI frame every 24ms\n");
    fprintf(out, "  -V                   : print version information and "
            "exit\n");
    fprintf(out, "  -z                   : write SCCA field for Factum ETI"
            " analyzer\n");
    fprintf(out, "\nINPUT OPTIONS\n");
    fprintf(out, "  -A                   : set audio service\n");
    fprintf(out, "  -B                   : set CRC-Bridge data service\n");
    fprintf(out, "  -D                   : set data service\n");
    fprintf(out, "  -E                   : set enhanced packet service\n");
    fprintf(out, "  -F                   : set DAB+ service\n");
    fprintf(out, "  -M                   : set DMB service\n");
    fprintf(out, "  -P                   : set packet service\n");
    fprintf(out, "  -T                   : set test service\n");
    fprintf(out, "  inputName<n>         : name of file for audio and "
            "packet service and Udp input address for data service "
            "([address]:port)\n");
    fprintf(out, "  -a                   : packet address (default: 0x%x "
            "(%i))\n", DEFAULT_PACKET_ADDRESS, DEFAULT_PACKET_ADDRESS);
    fprintf(out, "  -b bitrate<n>        : bitrate (in kbits/s) of the "
            "subchannel (default: audio 1st frame, data %i, packet %i)\n",
            DEFAULT_DATA_BITRATE, DEFAULT_PACKET_BITRATE);
    fprintf(out, "  -c                   : set the extendend country code ECC "
            "(default: %u (0x%2x)\n", DEFAULT_ENSEMBLE_ECC, DEFAULT_ENSEMBLE_ECC);
    fprintf(out, "  -d                   : turn on datagroups in packet "
            "mode\n");
    fprintf(out, "  -f figType           : user application type in FIG "
            "0/13 for packet mode\n");
    fprintf(out, "  -g language          : Primary service component "
            "language: english=9, french=15\n");
    fprintf(out, "  -i id<n>             : service|subchannel|"
            "serviceComponent id <n> (default: <n>)\n");
    fprintf(out, "  -k                   : set non-blocking file input "
            "(audio and packet only)\n");
    fprintf(out, "  -L label<n>          : label of service <n>"
            " (default: CRC-Audio<n>)\n");
    fprintf(out, "  -l sLabel<n>         : short label flag of service <n>"
            " (default: 0xf040)\n");
    fprintf(out, "  -p protection<n>     : protection level (default: 3)\n");
    fprintf(out, "  -s                   : enable TIST, synchronized on 1PPS at level 2.\n");
    fprintf(out, "                         This also transmits time using the MNSC.\n");
    fprintf(out, "  -t type              : audio/data service component type"
            " (default: 0)\n");
    fprintf(out, "                         audio: foreground=0, "
            "background=1, multi-channel=2\n");
    fprintf(out, "                         data: unspecified=0, TMC=1, "
            "EWS=2, ITTS=3, paging=4, TDC=5, DMB=24, IP=59, MOT=60, "
            "proprietary=61\n");
    fprintf(out, "  -y PTy               : Primary service component program"
            " type international code\n");
    fprintf(out, "\nOUTPUT OPTIONS\n");
    fprintf(out, "  -O output            : name of the output in format "
            "scheme://[address][:port][/name]\n"
            "                         where scheme is (raw|udp|tcp|file|fifo|simul)\n"
           );
}
#else // no command line options
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
#endif

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

        etiLog.log(info, " pty:           0x%x (%u)", service->pty,
                service->pty);

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

void printComponents(vector<DabComponent*>& components)
{
    vector<DabComponent*>::const_iterator current;
    unsigned int index = 0;

    for (current = components.begin(); current != components.end(); ++current) {
        etiLog.level(info) << "Component     " << (*current)->get_rc_name();
        printComponent(*current);
        ++index;
    }
}

void printComponent(DabComponent* component)
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

void printSubchannels(vector<DabSubchannel*>& subchannels)
{
    vector<DabSubchannel*>::iterator subchannel;
    int index = 0;

    for (subchannel = subchannels.begin(); subchannel != subchannels.end();
            ++subchannel) {
        dabProtection* protection = &(*subchannel)->protection;
        etiLog.level(info) << "Subchannel   " << (*subchannel)->uid;
        etiLog.log(info, " input");
        etiLog.level(info) << "   URI:     " << (*subchannel)->inputUri;
        switch ((*subchannel)->type) {
            case subchannel_type_t::Audio:
                etiLog.log(info, " type:       audio");
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
                (*subchannel)->id, (*subchannel)->id);
        etiLog.log(info, " bitrate:    %i",
                (*subchannel)->bitrate);
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
                    (*subchannel)->protection.level);
        }
        etiLog.log(info, " SAD:        %u",
                (*subchannel)->startAddress);
        etiLog.log(info, " size (CU):  %i",
                (*subchannel)->getSizeCu());
        ++index;
    }
}

void printEnsemble(const shared_ptr<dabEnsemble> ensemble)
{
    etiLog.log(info, "Ensemble");
    etiLog.log(info, " id:          0x%lx (%lu)", ensemble->id, ensemble->id);
    etiLog.log(info, " ecc:         0x%x (%u)", ensemble->ecc, ensemble->ecc);
    etiLog.level(info) << " label:       " <<
            ensemble->label.long_label();
    etiLog.level(info) << " short label: " <<
            ensemble->label.short_label();

    etiLog.log(info, " (0x%x)", ensemble->label.flag());
    etiLog.log(info, " mode:        %u", ensemble->mode);

    if (ensemble->lto_auto) {
        time_t now = time(NULL);
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
        for (const auto& cluster : ensemble->clusters) {
            etiLog.level(info) << cluster->tostring();
        }
    }
}

