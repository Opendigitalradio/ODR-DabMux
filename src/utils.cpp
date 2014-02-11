/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2013,2014
   Matthias P. Braendli, http://mpb.li, matthias.braendli@mpb.li
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
#include "DabMux.h"
#include "utils.h"


time_t getDabTime()
{
    static time_t oldTime = 0;
    static int offset = 0;
    if (oldTime == 0) {
        oldTime = time(NULL);
    } else {
        offset+= 24;
        if (offset >= 1000) {
            offset -= 1000;
            ++oldTime;
        }
    }
    return oldTime;
}


void header_message() 
{
    etiLog.log(info,
            "Welcome to %s %s, compiled at %s, %s\n\n",
            PACKAGE_NAME,
#if defined(GITVERSION)
            GITVERSION,
#else
            PACKAGE_VERSION,
#endif
            __DATE__, __TIME__);
    etiLog.log(info,
            "Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012\n"
            "Her Majesty the Queen in Right of Canada,\n"
            "(Communications Research Centre Canada) All rights reserved.\n\n"
            "Copyright (C) 2013,2014\nMatthias P. Braendli, http://mpb.li\n\n");

    etiLog.level(info) << "Input URLs supported:" <<
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
    "\n";

    etiLog.level(info) << "Inputs format supported:"
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
    "\n";

    etiLog.level(info) << "Output URLs supported:"
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
    "\n\n";

}

void printUsage(char *name, FILE* out)
{
    fprintf(out, "NAME\n");
    fprintf(out, "  %s - A software DAB multiplexer\n", name);
    fprintf(out, "\nSYNOPSYS\n");
    fprintf(out, "    You can use either a configuration file, or the command line arguments\n");
    fprintf(out, "    With external configuration file:\n");
    fprintf(out, "  %s -e configuration.mux\n", name);
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
            "  |                   CRC-Ensemble                   |  ENSEMBLE\n"
            "  |__________________________________________________|\n"
            "          |                 |                 |\n"
            "          |                 |                 |\n"
            "   _______V______    _______V______    _______V______\n"
            "  | CRC-Service1 |  | CRC-Service2 |  | CRC-Service3 |  SERVICES\n"
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
    fprintf(out, "  -s                   : enable TIST, synchronized on 1PPS at level 2. This also transmits time using the MNSC.\n");
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

void printOutputs(vector<dabOutput*>& outputs)
{
    vector<dabOutput*>::const_iterator output;
    int index = 0;

    for (output = outputs.begin(); output != outputs.end(); ++output) {
        etiLog.log(info, "Output      %i\n", index);
        etiLog.log(info, "  protocol: %s\n",
                (*output)->outputProto.c_str());
        etiLog.log(info, "  name:     %s\n",
                (*output)->outputName.c_str());
        ++index;
    }
}

void printServices(vector<dabService*>& services)
{
    vector<dabService*>::const_iterator current;
    int index = 0;

    for (current = services.begin(); current != services.end(); ++current) {
        char label[17];
        memcpy(label, (*current)->label.text, 16);
        label[16] = 0;

        etiLog.log(info, "Service       %i\n", index);
        etiLog.log(info, " label:       %s\n", label);
        etiLog.log(info, " short label: ");
        {
            LogLine line = etiLog.level(info);
            for (int i = 0; i < 32; ++i) {
                if ((*current)->label.flag & 0x8000 >> i) {
                    line << (*current)->label.text[i];
                }
            }
        }
        etiLog.log(info, " (0x%x)\n", (*current)->label.flag);
        etiLog.log(info, " id:          0x%lx (%lu)\n",
                (*current)->id, (*current)->id);
        etiLog.log(info, " pty:         0x%x (%u)\n",
                (*current)->pty, (*current)->pty);
        etiLog.log(info, " language:    0x%x (%u)\n",
                (*current)->language, (*current)->language);
        ++index;
    }
}

void printComponents(vector<dabComponent*>& components)
{
    vector<dabComponent*>::const_iterator current;
    unsigned int index = 0;

    for (current = components.begin(); current != components.end(); ++current) {
        etiLog.log(info, "Component       %i\n", index);
        printComponent(*current);
        ++index;
    }
}

void printComponent(dabComponent* component)
{
    char label[17];
    memcpy(label, component->label.text, 16);
    label[16] = 0;
    if (label[0] == 0) {
        sprintf(label, "<none>");
    }

    etiLog.log(info, " service id:             %i\n",
            component->serviceId);
    etiLog.log(info, " subchannel id:          %i\n",
            component->subchId);
    etiLog.log(info, " label:                  %s\n",
            label);
    etiLog.log(info, " short label:            ");
    {
        LogLine line = etiLog.level(info);
        for (int i = 0; i < 32; ++i) {
            if (component->label.flag & 0x8000 >> i) {
                line << component->label.text[i];
            }
        }
    }
    etiLog.log(info, " (0x%x)\n", component->label.flag);
    etiLog.log(info, " service component type: 0x%x (%u)\n",
            component->type, component->type);
    etiLog.log(info, " (packet) id:            %u\n",
            component->packet.id);
    etiLog.log(info, " (packet) address:       %u\n",
            component->packet.address);
    etiLog.log(info, " (packet) app type:      %u\n",
            component->packet.appType);
    etiLog.log(info, " (packet) datagroup:     %u\n",
            component->packet.datagroup);
}

void printSubchannels(vector<dabSubchannel*>& subchannels)
{
    vector<dabSubchannel*>::iterator subchannel;
    int index = 0;

    for (subchannel = subchannels.begin(); subchannel != subchannels.end();
            ++subchannel) {
        dabProtection* protection = &(*subchannel)->protection;
        etiLog.log(info, "Subchannel   %i\n", index);
        etiLog.log(info, " input\n");
        etiLog.log(info, "   protocol: %s\n",
                (*subchannel)->inputProto);
        etiLog.log(info, "   name:     %s\n",
                (*subchannel)->inputName);
        etiLog.log(info, " type:       ");
        switch ((*subchannel)->type) {
        case 0:
            etiLog.log(info, "audio\n");
            break;
        case 1:
            etiLog.log(info, "data\n");
            break;
        case 2:
            etiLog.log(info, "fidc\n");
            break;
        case 3:
            etiLog.log(info, "packet\n");
            break;
        default:
            etiLog.log(info, "Unknown data type "
                    "(service->type)\n");
            break;
        }
        etiLog.log(info, " id:         %i\n",
                (*subchannel)->id);
        etiLog.log(info, " bitrate:    %i\n",
                (*subchannel)->bitrate);
        if (protection->form == 0) {
            etiLog.log(info, " protection: UEP %i\n", protection->level + 1);
        } else {
            etiLog.log(info, " protection: EEP %i-%c\n",
                    protection->level + 1,
                    protection->longForm.option == 0 ? 'A' : 'B');
        }
        if (protection->form == 0) {
            etiLog.log(info,
                    "  form:      short\n  switch:    %i\n  index:     %i\n",
                    protection->shortForm.tableSwitch,
                    protection->shortForm.tableIndex);
        } else {
            etiLog.log(info,
                    "  form:      long\n  option:    %i\n  level:     %i\n",
                    protection->longForm.option,
                    (*subchannel)->protection.level);
        }
        etiLog.log(info, " SAD:        %i\n",
                (*subchannel)->startAddress);
        etiLog.log(info, " size (CU):  %i\n",
                getSizeCu(*subchannel));
        ++index;
    }
}

void printEnsemble(dabEnsemble* ensemble)
{
    char label[17];
    memcpy(label, ensemble->label.text, 16);
    label[16] = 0;

    etiLog.log(info, "Ensemble\n");
    etiLog.log(info, " id:          0x%lx (%lu)\n",
            ensemble->id, ensemble->id);
    etiLog.log(info, " ecc:         0x%x (%u)\n",
            ensemble->ecc, ensemble->ecc);
    etiLog.log(info, " label:       %s\n", label);
    {
        LogLine line = etiLog.level(info);
        line << " short label: ";
        for (int i = 0; i < 32; ++i) {
            if (ensemble->label.flag & 0x8000 >> i) {
                line << ensemble->label.text[i];
            }
        }
    }
    etiLog.log(info, " (0x%x)\n", ensemble->label.flag);
    etiLog.log(info, " mode:        %u\n", ensemble->mode);
}



