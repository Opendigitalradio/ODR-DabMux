/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)
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


#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <cstdio>
#include <cstring>
#include <errno.h>

using namespace std;
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

#   include <linux/if_packet.h>
#   include <linux/netdevice.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

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


#include "dabOutput.h"
#include "crc.h"
#include "UdpSocket.h"
#include "InetAddress.h"
#include "dabUtils.h"
#include "PcDebug.h"


// DAB Mode
#define DEFAULT_DAB_MODE    2

// Taille de la trame de donnee, sous-canal 3, nb de paquets de 64bits,
// STL3 * 8 = x kbytes par trame ETI

// Data bitrate in kbits/s. Must be 64 kb/s multiple.
#define DEFAULT_DATA_BITRATE    384
#define DEFAULT_PACKET_BITRATE  32

// Etiquettes des sous-canaux et de l'ensemble, 16 characteres incluant les
// espaces
#define DEFAULT_ENSEMBLE_LABEL          "CRC-Dr.Radio"
#define DEFAULT_ENSEMBLE_SHORT_LABEL    0xe000
#define DEFAULT_ENSEMBLE_ID             0xc000
#define DEFAULT_ENSEMBLE_ECC            0xa1

//Numeros des sous-canaux
#define DEFAULT_SERVICE_ID      50
#define DEFAULT_PACKET_ADDRESS  0


struct dabOutput {
    const char* outputProto;
    const char* outputName;
    void* data;
    dabOutputOperations operations;
};


struct dabLabel {
    char text[16];
    uint16_t flag;
};


struct dabService;
struct dabComponent;
struct dabSubchannel;
struct dabEnsemble {
    uint16_t id;
    uint8_t ecc;
    dabLabel label;
    uint8_t mode;
    vector<dabService*> services;
    vector<dabComponent*> components;
    vector<dabSubchannel*> subchannels;
};


struct dabProtectionShort {
    unsigned char tableSwitch;
    unsigned char tableIndex;
};


struct dabProtectionLong {
    unsigned char option;
};


struct dabProtection {
    unsigned char level;
    unsigned char form;
    union {
        dabProtectionShort shortForm;
        dabProtectionLong longForm;
    };
};


struct dabSubchannel {
    const char* inputProto;
    const char* inputName;
    void* data;
    dabInputOperations operations;
    unsigned char id;
    unsigned char type;
    uint16_t startAddress;
    uint16_t bitrate;
    dabProtection protection;
};


class SubchannelId : public std::binary_function <dabSubchannel*, int, bool> {
public:
    bool operator()(const dabSubchannel* subchannel, const int id) const {
        return subchannel->id == id;
    }
};


vector<dabSubchannel*>::iterator getSubchannel(
        vector<dabSubchannel*>& subchannels, int id)
{
    return find_if(
            subchannels.begin(),
            subchannels.end(),
            bind2nd(SubchannelId(), id)
            );
}


struct dabAudioComponent {
};


struct dabDataComponent {
};


struct dabFidcComponent {
};


struct dabPacketComponent {
    uint16_t id;
    uint16_t address;
    uint16_t appType;
    bool datagroup;
};


struct dabComponent {
    dabLabel label;
    uint32_t serviceId;
    uint8_t subchId;
    uint8_t type;
    uint8_t SCIdS;
    union {
        dabAudioComponent audio;
        dabDataComponent data;
        dabFidcComponent fidc;
        dabPacketComponent packet;
    };

    bool isPacketComponent(vector<dabSubchannel*>& subchannels)
    {
        if (subchId > 63) {
            etiLog.printHeader(TcpLog::ERR,
                    "You must define subchannel id in the "
                    "packet component before defining packet ");
            return false;
        }
        if (getSubchannel(subchannels, subchId) == subchannels.end()) {
            etiLog.printHeader(TcpLog::ERR,
                    "Invalid subchannel id in the packet component "
                    "for defining packet ");
            return false;
        }
        if ((*getSubchannel(subchannels, subchId))->type != 3) {
            etiLog.printHeader(TcpLog::ERR,
                    "Invalid component type for defining packet ");
            return false;
        }
        return true;
    }
};


vector<dabComponent*>::iterator getComponent(
        vector<dabComponent*>& components,
        uint32_t serviceId,
        vector<dabComponent*>::iterator current)
{
    if (current == components.end()) {
        current = components.begin();
    } else {
        ++current;
    }

    while (current != components.end()) {
        if ((*current)->serviceId == serviceId) {
            return current;
        }
        ++current;
    }

    return components.end();
}


vector<dabComponent*>::iterator getComponent(
        vector<dabComponent*>& components,
        uint32_t serviceId) {
    return getComponent(components, serviceId, components.end());
}


struct dabService {
    dabLabel label;
    uint32_t id;
    unsigned char pty;
    unsigned char language;
    bool program;

    unsigned char getType(dabEnsemble* ensemble)
    {
        vector<dabSubchannel*>::iterator subchannel;
        vector<dabComponent*>::iterator component =
            getComponent(ensemble->components, id);
        if (component == ensemble->components.end()) {
            return 4;
        }
        subchannel = getSubchannel(ensemble->subchannels, (*component)->subchId);
        if (subchannel == ensemble->subchannels.end()) {
            return 8;
        }

        return (*subchannel)->type;
    }
    unsigned char nbComponent(vector<dabComponent*>& components)
    {
        int nb = 0;
        vector<dabComponent*>::iterator current;

        for (current = components.begin(); current != components.end();
                ++current) {
            if ((*current)->serviceId == id) {
                ++nb;
            }
        }
        return nb;
    }
};


vector<dabService*>::iterator getService(
        dabComponent* component,
        vector<dabService*>& services)
{
    vector<dabService*>::iterator service;

    for (service = services.begin(); service != services.end(); ++service) {
        if ((*service)->id == component->serviceId) {
            break;
        }
    }

    return service;
}


/******************************************************************************
 *****************   Definitions des stuctures des FIGs  **********************
 ******************************************************************************/
struct FIGtype0 {
    uint8_t Length:5;
    uint8_t FIGtypeNumber:3;
    uint8_t Extension:5;
    uint8_t PD:1;
    uint8_t OE:1;
    uint8_t CN:1;
} PACKED;


struct FIGtype0_0 {
    uint8_t Length:5;
    uint8_t FIGtypeNumber:3;
    uint8_t Extension:5;
    uint8_t PD:1;
    uint8_t OE:1;
    uint8_t CN:1;

    uint16_t EId;
    uint8_t CIFcnt_hight:5;
    uint8_t Al:1;
    uint8_t Change:2;
    uint8_t CIFcnt_low:8;
} PACKED;


struct FIGtype0_2 {
    uint8_t Length:5;
    uint8_t FIGtypeNumber:3;
    uint8_t Extension:5;
    uint8_t PD:1;
    uint8_t OE:1;
    uint8_t CN:1;
} PACKED;


struct FIGtype0_2_Service {
    uint16_t SId;
    uint8_t NbServiceComp:4;
    uint8_t CAId:3;
    uint8_t Local_flag:1;
} PACKED;


struct FIGtype0_2_Service_data {
    uint32_t SId;
    uint8_t NbServiceComp:4;
    uint8_t CAId:3;
    uint8_t Local_flag:1;
} PACKED;


struct FIGtype0_2_audio_component {
    uint8_t ASCTy:6;
    uint8_t TMid:2;
    uint8_t CA_flag:1;
    uint8_t PS:1;
    uint8_t SubChId:6;
} PACKED;


struct FIGtype0_2_data_component {
    uint8_t DSCTy:6;
    uint8_t TMid:2;
    uint8_t CA_flag:1;
    uint8_t PS:1;
    uint8_t SubChId:6;
} PACKED;


struct FIGtype0_2_packet_component {
    uint8_t SCId_high:6;
    uint8_t TMid:2;
    uint8_t CA_flag:1;
    uint8_t PS:1;
    uint8_t SCId_low:6;
    void setSCId(uint16_t SCId) {
        SCId_high = SCId >> 6;
        SCId_low = SCId & 0x3f;
    }
} PACKED;


struct FIGtype0_3_header {
    uint8_t Length:5;
    uint8_t FIGtypeNumber:3;
    uint8_t Extension:5;
    uint8_t PD:1;
    uint8_t OE:1;
    uint8_t CN:1;
} PACKED;


/* Warning: When bit SCCA_flag is unset(0), the multiplexer R&S does not send
 * the SCCA field. But, in the Factum ETI analyzer, if this field is not there,
 * it is an error.
 */
struct FIGtype0_3_data {
    uint8_t SCId_high;
    uint8_t SCCA_flag:1;
    uint8_t rfa:3;
    uint8_t SCId_low:4;
    uint8_t DSCTy:6;
    uint8_t rfu:1;
    uint8_t DG_flag:1;
    uint8_t Packet_address_high:2;
    uint8_t SubChId:6;
    uint8_t Packet_address_low;
    uint16_t SCCA;
    void setSCId(uint16_t SCId) {
        SCId_high = SCId >> 4;
        SCId_low = SCId & 0xf;
    }
    void setPacketAddress(uint16_t address) {
        Packet_address_high = address >> 8;
        Packet_address_low = address & 0xff;
    }
} PACKED;


struct FIGtype0_8_short {
    uint8_t SCIdS:4;
    uint8_t rfa_1:3;
    uint8_t ext:1;
    uint8_t Id:6;
    uint8_t MscFic:1;
    uint8_t LS:1;
    uint8_t rfa_2;
} PACKED;


struct FIGtype0_8_long {
    uint8_t SCIdS:4;
    uint8_t rfa_1:3;
    uint8_t ext:1;
    uint8_t SCId_high:4;
    uint8_t rfa:3;
    uint8_t LS:1;
    uint8_t SCId_low;
    uint8_t rfa_2;
    void setSCId(uint16_t id) {
        SCId_high = id >> 8;
        SCId_low = id & 0xff;
    }
    uint16_t getSCid() {
        return (SCId_high << 8) | SCId_low;
    }
} PACKED;


struct FIGtype0_9 {
    uint8_t Length:5;
    uint8_t FIGtypeNumber:3;
    uint8_t Extension:5;
    uint8_t PD:1;
    uint8_t OE:1;
    uint8_t CN:1;

    uint8_t ensembleLto:6;
    uint8_t lto:1;
    uint8_t ext:1;
    uint8_t ensembleEcc;
    uint8_t tableId;
} PACKED;


struct FIGtype0_10 {
    uint8_t Length:5;
    uint8_t FIGtypeNumber:3;
    uint8_t Extension:5;
    uint8_t PD:1;
    uint8_t OE:1;
    uint8_t CN:1;

    uint8_t MJD_high:7;
    uint8_t RFU:1;
    uint8_t MJD_med;
    uint8_t Hours_high:3;
    uint8_t UTC:1;
    uint8_t ConfInd:1;
    uint8_t LSI:1;
    uint8_t MJD_low:2;
    uint8_t Minutes:6;
    uint8_t Hours_low:2;
    void setMJD(uint32_t date) {
        MJD_high = (date >> 10) & 0x7f;
        MJD_med = (date >> 2) & 0xff;
        MJD_low = date & 0x03;
    }
    void setHours(uint16_t hours) {
        Hours_high = (hours >> 2) & 0x07;
        Hours_low = hours & 0x03;
    }
} PACKED;


struct FIGtype0_17_programme {
    uint16_t SId;
    uint8_t NFC:2;
    uint8_t Rfa:2;
    uint8_t CC:1;
    uint8_t L:1;
    uint8_t PS:1;
    uint8_t SD:1;
} PACKED;


struct FIGtype0_1 {
    uint8_t Length:5;
    uint8_t FIGtypeNumber:3;
    uint8_t Extension:5;
    uint8_t PD:1;
    uint8_t OE:1;
    uint8_t CN:1;
} PACKED;


struct FIG_01_SubChannel_ShortF {
    uint8_t StartAdress_high:2;
    uint8_t SubChId:6;
    uint8_t StartAdress_low:8;
    uint8_t TableIndex:6;
    uint8_t TableSwitch:1;
    uint8_t Short_Long_form:1;
} PACKED;


struct FIG_01_SubChannel_LongF {
    uint8_t StartAdress_high:2;
    uint8_t SubChId:6;
    uint8_t StartAdress_low:8;
    uint8_t Sub_ChannelSize_high:2;
    uint8_t ProtectionLevel:2;
    uint8_t Option:3;
    uint8_t Short_Long_form:1;
    uint8_t Sub_ChannelSize_low:8;
} PACKED;


struct FIG0_13_shortAppInfo {
    uint16_t SId;
    uint8_t No:4;
    uint8_t SCIdS:4;
} PACKED;


struct FIG0_13_longAppInfo {
    uint32_t SId;
    uint8_t No:4;
    uint8_t SCIdS:4;
} PACKED;


struct FIG0_13_app {
    uint8_t typeHigh;
    uint8_t length:5;
    uint8_t typeLow:3;
    void setType(uint16_t type) {
        typeHigh = type >> 3;
        typeLow = type & 0x1f;
    }
} PACKED;


struct FIGtype1_0 {
    uint8_t Length:5;
    uint8_t FIGtypeNumber:3;
    uint8_t Extension:3;
    uint8_t OE:1;
    uint8_t Charset:4;

    uint16_t EId;
} PACKED;


struct FIGtype1_1 {
    uint8_t Length:5;
    uint8_t FIGtypeNumber:3;
    uint8_t Extension:3;
    uint8_t OE:1;
    uint8_t Charset:4;

    uint16_t Sld;
} PACKED;


struct FIGtype1_5 {
    uint8_t Length:5;
    uint8_t FIGtypeNumber:3;
    uint8_t Extension:3;
    uint8_t OE:1;
    uint8_t Charset:4;
    uint32_t SId;
} PACKED;


struct FIGtype1_4_programme {
    uint8_t Length:5;
    uint8_t FIGtypeNumber:3;
    uint8_t Extension:3;
    uint8_t OE:1;
    uint8_t Charset:4;
    uint8_t SCIdS:4;
    uint8_t rfa:3;
    uint8_t PD:1;
    uint16_t SId;
} PACKED;


struct FIGtype1_4_data {
    uint8_t Length:5;
    uint8_t FIGtypeNumber:3;
    uint8_t Extension:3;
    uint8_t OE:1;
    uint8_t Charset:4;
    uint8_t SCIdS:4;
    uint8_t rfa:3;
    uint8_t PD:1;
    uint32_t SId;
} PACKED;


#ifdef _WIN32
#   pragma pack(pop)
#endif

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


const unsigned short Sub_Channel_SizeTable[64] = {
    16, 21, 24, 29, 35, 24, 29, 35,
    42, 52, 29, 35, 42, 52, 32, 42,
    48, 58, 70, 40, 52, 58, 70, 84,
    48, 58, 70, 84, 104, 58, 70, 84,
    104, 64, 84, 96, 116, 140, 80, 104,
    116, 140, 168, 96, 116, 140, 168, 208,
    116, 140, 168, 208, 232, 128, 168, 192,
    232, 280, 160, 208, 280, 192, 280, 416
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


unsigned short getSizeByte(dabSubchannel* subchannel)
{
    return subchannel->bitrate * 3;
}


unsigned short getSizeWord(dabSubchannel* subchannel)
{
    return (subchannel->bitrate * 3) >> 2;
}


unsigned short getSizeDWord(dabSubchannel* subchannel)
{
    return (subchannel->bitrate * 3) >> 3;
}


unsigned short getSizeCu(dabSubchannel* subchannel)
{
    if (subchannel->protection.form == 0) {
        return Sub_Channel_SizeTable[subchannel->
            protection.shortForm.tableIndex];
    } else {
        dabProtectionLong* protection =
            &subchannel->protection.longForm;
        switch (protection->option) {
        case 0:
            switch (subchannel->protection.level) {
            case 0:
                return (subchannel->bitrate * 12) >> 3;
                break;
            case 1:
                return subchannel->bitrate;
                break;
            case 2:
                return (subchannel->bitrate * 6) >> 3;
                break;
            case 3:
                return (subchannel->bitrate >> 1);
                break;
            default: // Should not happens
                etiLog.print(TcpLog::ERR, "Bad protection level on "
                        "subchannel\n");
                return 0;
            }
            break;
        case 1:
            switch (subchannel->protection.level) {
            case 0:
                return (subchannel->bitrate * 27) >> 5;
                break;
            case 1:
                return (subchannel->bitrate * 21) >> 5;
                break;
            case 2:
                return (subchannel->bitrate * 18) >> 5;
                break;
            case 3:
                return (subchannel->bitrate * 15) >> 5;
                break;
            default: // Should not happens
                etiLog.print(TcpLog::ERR,
                        "Bad protection level on subchannel\n");
                return 0;
            }
            break;
        default:
            etiLog.print(TcpLog::ERR, "Invalid protection option\n");
            return 0;
        }
    }
    return 0;
}


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


void printUsage(char *name, FILE* out = stderr)
{
    fprintf(out, "NAME\n");
    fprintf(out, "  %s - A software DAB multiplexer\n", name);
    fprintf(out, "\nSYNOPSYS\n");
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
            "  %s  is  a  software  multiplexer  that generates an ETI stream from\n"
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
    fprintf(out, "  -o                   : turn on TCP log on port 12222\n");
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
            "(default: %u (0x%2x)\n");
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
    fprintf(out, "  -s                   : enable TIST, synchronized on 1PPS at level 2\n");
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


bool running = true;

void signalHandler(int signum)
{
#ifdef _WIN32
    etiLog.print(TcpLog::DBG, "\npid: %i\n", _getpid());
#else
    etiLog.print(TcpLog::DBG, "\npid: %i, ppid: %i\n", getpid(), getppid());
#endif
    etiLog.print(TcpLog::DBG, "Signal handler called with signal ");
    switch (signum) {
#ifndef _WIN32
    case SIGHUP:
        etiLog.print(TcpLog::DBG, "SIGHUP\n");
        break;
    case SIGQUIT:
        etiLog.print(TcpLog::DBG, "SIGQUIT\n");
        break;
    case SIGPIPE:
        etiLog.print(TcpLog::DBG, "SIGPIPE\n");
        return;
        break;
#endif
    case SIGINT:
        etiLog.print(TcpLog::DBG, "SIGINT\n");
        break;
    case SIGTERM:
        etiLog.print(TcpLog::DBG, "SIGTERM\n");
        etiLog.print(TcpLog::DBG, "Exiting software\n");
        exit(0);
        break;
    default:
        etiLog.print(TcpLog::DBG, "number %i\n", signum);
    }
#ifndef _WIN32
    killpg(0, SIGPIPE);
#endif
    running = false;
}


void printEnsemble(dabEnsemble* ensemble)
{
    char label[17];
    memcpy(label, ensemble->label.text, 16);
    label[16] = 0;

    etiLog.printHeader(TcpLog::INFO, "Ensemble\n");
    etiLog.printHeader(TcpLog::INFO, " id:          0x%lx (%lu)\n",
            ensemble->id, ensemble->id);
    etiLog.printHeader(TcpLog::INFO, " ecc:         0x%x (%u)\n",
            ensemble->ecc, ensemble->ecc);
    etiLog.printHeader(TcpLog::INFO, " label:       %s\n", label);
    etiLog.printHeader(TcpLog::INFO, " short label: ");
    for (int i = 0; i < 32; ++i) {
        if (ensemble->label.flag & 0x8000 >> i) {
            etiLog.printHeader(TcpLog::INFO, "%c", ensemble->label.text[i]);
        }
    }
    etiLog.printHeader(TcpLog::INFO, " (0x%x)\n", ensemble->label.flag);
    etiLog.printHeader(TcpLog::INFO, " mode:        %u\n", ensemble->mode);
}


void printSubchannels(vector<dabSubchannel*>& subchannels)
{
    vector<dabSubchannel*>::iterator subchannel;
    int index = 0;

    for (subchannel = subchannels.begin(); subchannel != subchannels.end();
            ++subchannel) {
        dabProtection* protection = &(*subchannel)->protection;
        etiLog.printHeader(TcpLog::INFO, "Subchannel   %i\n", index);
        etiLog.printHeader(TcpLog::INFO, " input\n");
        etiLog.printHeader(TcpLog::INFO, "   protocol: %s\n",
                (*subchannel)->inputProto);
        etiLog.printHeader(TcpLog::INFO, "   name:     %s\n",
                (*subchannel)->inputName);
        etiLog.printHeader(TcpLog::INFO, " type:       ");
        switch ((*subchannel)->type) {
        case 0:
            etiLog.printHeader(TcpLog::INFO, "audio\n");
            break;
        case 1:
            etiLog.printHeader(TcpLog::INFO, "data\n");
            break;
        case 2:
            etiLog.printHeader(TcpLog::INFO, "fidc\n");
            break;
        case 3:
            etiLog.printHeader(TcpLog::INFO, "packet\n");
            break;
        default:
            etiLog.printHeader(TcpLog::INFO, "Unknown data type "
                    "(service->type)\n");
            break;
        }
        etiLog.printHeader(TcpLog::INFO, " id:         %i\n",
                (*subchannel)->id);
        etiLog.printHeader(TcpLog::INFO, " bitrate:    %i\n",
                (*subchannel)->bitrate);
        etiLog.printHeader(TcpLog::INFO, " protection: ");
        if (protection->form == 0) {
            etiLog.printHeader(TcpLog::INFO, "UEP %i\n", protection->level + 1);
        } else {
            etiLog.printHeader(TcpLog::INFO, "EEP %i-%c\n",
                    protection->level + 1,
                    protection->longForm.option == 0 ? 'A' : 'B');
        }
        if (protection->form == 0) {
            etiLog.printHeader(TcpLog::INFO,
                    "  form:      short\n  switch:    %i\n  index:     %i\n",
                    protection->shortForm.tableSwitch,
                    protection->shortForm.tableIndex);
        } else {
            etiLog.printHeader(TcpLog::INFO,
                    "  form:      long\n  option:    %i\n  level:     %i\n",
                    protection->longForm.option,
                    (*subchannel)->protection.level);
        }
        etiLog.printHeader(TcpLog::INFO, " SAD:        %i\n",
                (*subchannel)->startAddress);
        etiLog.printHeader(TcpLog::INFO, " size (CU):  %i\n",
                getSizeCu(*subchannel));
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

    etiLog.printHeader(TcpLog::INFO, " service id:             %i\n",
            component->serviceId);
    etiLog.printHeader(TcpLog::INFO, " subchannel id:          %i\n",
            component->subchId);
    etiLog.printHeader(TcpLog::INFO, " label:                  %s\n",
            label);
    etiLog.printHeader(TcpLog::INFO, " short label:            ");
    for (int i = 0; i < 32; ++i) {
        if (component->label.flag & 0x8000 >> i) {
            etiLog.printHeader(TcpLog::INFO, "%c", component->label.text[i]);
        }
    }
    etiLog.printHeader(TcpLog::INFO, " (0x%x)\n", component->label.flag);
    etiLog.printHeader(TcpLog::INFO, " service component type: 0x%x (%u)\n",
            component->type, component->type);
    etiLog.printHeader(TcpLog::INFO, " (packet) id:            %u\n",
            component->packet.id);
    etiLog.printHeader(TcpLog::INFO, " (packet) address:       %u\n",
            component->packet.address);
    etiLog.printHeader(TcpLog::INFO, " (packet) app type:      %u\n",
            component->packet.appType);
    etiLog.printHeader(TcpLog::INFO, " (packet) datagroup:     %u\n",
            component->packet.datagroup);
}


void printComponents(vector<dabComponent*>& components)
{
    vector<dabComponent*>::const_iterator current;
    unsigned int index = 0;

    for (current = components.begin(); current != components.end(); ++current) {
        etiLog.printHeader(TcpLog::INFO, "Component       %i\n", index);
        printComponent(*current);
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

        etiLog.printHeader(TcpLog::INFO, "Service       %i\n", index);
        etiLog.printHeader(TcpLog::INFO, " label:       %s\n", label);
        etiLog.printHeader(TcpLog::INFO, " short label: ");
        for (int i = 0; i < 32; ++i) {
            if ((*current)->label.flag & 0x8000 >> i) {
                etiLog.printHeader(TcpLog::INFO, "%c", (*current)->label.text[i]);
            }
        }
        etiLog.printHeader(TcpLog::INFO, " (0x%x)\n", (*current)->label.flag);
        etiLog.printHeader(TcpLog::INFO, " id:          0x%lx (%lu)\n",
                (*current)->id, (*current)->id);
        etiLog.printHeader(TcpLog::INFO, " pty:         0x%x (%u)\n",
                (*current)->pty, (*current)->pty);
        etiLog.printHeader(TcpLog::INFO, " language:    0x%x (%u)\n",
                (*current)->language, (*current)->language);
        ++index;
    }
}


void printOutputs(vector<dabOutput*>& outputs)
{
    vector<dabOutput*>::const_iterator output;
    int index = 0;

    for (output = outputs.begin(); output != outputs.end(); ++output) {
        etiLog.printHeader(TcpLog::INFO, "Output      %i\n", index);
        etiLog.printHeader(TcpLog::INFO, "  protocol: %s\n",
                (*output)->outputProto);
        etiLog.printHeader(TcpLog::INFO, "  name:     %s\n",
                (*output)->outputName);
        ++index;
    }
}


int main(int argc, char *argv[])
{
    etiLog.printHeader(TcpLog::INFO,
            "Welcome to %s %s, compiled at %s, %s\n\n",
            PACKAGE_NAME, PACKAGE_VERSION, __DATE__, __TIME__);
    etiLog.printHeader(TcpLog::INFO,
            "Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011\n"
            "Her Majesty the Queen in Right of Canada,\n"
            "(Communications Research Centre Canada) All rights reserved.\n\n");
    etiLog.printHeader(TcpLog::INFO, "Input URLs supported:");
#if defined(HAVE_INPUT_PRBS)
    etiLog.printHeader(TcpLog::INFO, " prbs");
#endif
#if defined(HAVE_INPUT_TEST)
    etiLog.printHeader(TcpLog::INFO, " test");
#endif
#if defined(HAVE_INPUT_SLIP)
    etiLog.printHeader(TcpLog::INFO, " slip");
#endif
#if defined(HAVE_INPUT_UDP)
    etiLog.printHeader(TcpLog::INFO, " udp");
#endif
#if defined(HAVE_INPUT_FIFO)
    etiLog.printHeader(TcpLog::INFO, " fifo");
#endif
#if defined(HAVE_INPUT_FILE)
    etiLog.printHeader(TcpLog::INFO, " file");
#endif
    etiLog.printHeader(TcpLog::INFO, "\n");

    etiLog.printHeader(TcpLog::INFO, "Inputs format supported:");
#if defined(HAVE_FORMAT_RAW)
    etiLog.printHeader(TcpLog::INFO, " raw");
#endif
#if defined(HAVE_FORMAT_BRIDGE)
    etiLog.printHeader(TcpLog::INFO, " bridge");
#endif
#if defined(HAVE_FORMAT_MPEG)
    etiLog.printHeader(TcpLog::INFO, " mpeg");
#endif
#if defined(HAVE_FORMAT_PACKET)
    etiLog.printHeader(TcpLog::INFO, " packet");
#endif
#if defined(HAVE_FORMAT_DMB)
    etiLog.printHeader(TcpLog::INFO, " dmb");
#endif
#if defined(HAVE_FORMAT_EPM)
    etiLog.printHeader(TcpLog::INFO, " epm");
#endif
    etiLog.printHeader(TcpLog::INFO, "\n");

    etiLog.printHeader(TcpLog::INFO, "Output URLs supported:");
#if defined(HAVE_OUTPUT_FILE)
    etiLog.printHeader(TcpLog::INFO, " file");
#endif
#if defined(HAVE_OUTPUT_FIFO)
    etiLog.printHeader(TcpLog::INFO, " fifo");
#endif
#if defined(HAVE_OUTPUT_UDP)
    etiLog.printHeader(TcpLog::INFO, " udp");
#endif
#if defined(HAVE_OUTPUT_TCP)
    etiLog.printHeader(TcpLog::INFO, " tcp");
#endif
#if defined(HAVE_OUTPUT_RAW)
    etiLog.printHeader(TcpLog::INFO, " raw");
#endif
#if defined(HAVE_OUTPUT_SIMUL)
    etiLog.printHeader(TcpLog::INFO, " simul");
#endif
    etiLog.printHeader(TcpLog::INFO, "\n\n");

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
        etiLog.printHeader(TcpLog::WARNING, "Can't increase priority: %s\n",
                strerror(errno));
    }
#else
    if (setpriority(PRIO_PROCESS, 0, -20) == -1) {
        etiLog.printHeader(TcpLog::WARNING, "Can't increase priority: %s\n",
                strerror(errno));
    }
#endif
    /*sched_param scheduler;
      scheduler.sched_priority = 99;        // sched_get_priority_max(SCHED_RR)
      if (sched_setscheduler(0, SCHED_RR, &scheduler) == -1) {
      etiLog.print(TcpLog::WARNING, "Can't increased priority: %s\n",
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
    strncpy(ensemble->label.text, DEFAULT_ENSEMBLE_LABEL, 16);
    ensemble->mode = DEFAULT_DAB_MODE;
    ensemble->label.flag = DEFAULT_ENSEMBLE_SHORT_LABEL;
    ensemble->id = DEFAULT_ENSEMBLE_ID;
    ensemble->ecc = DEFAULT_ENSEMBLE_ECC;

    vector<dabOutput*> outputs;
    vector<dabService*>::iterator service = ensemble->services.end();
    vector<dabService*>::iterator serviceProgramInd;
    vector<dabService*>::iterator serviceDataInd;
    vector<dabService*>::iterator servicePty;
    vector<dabComponent*>::iterator component = ensemble->components.end();
    vector<dabComponent*>::iterator componentIndicatorProgram;
    vector<dabComponent*>::iterator componentIndicatorData;
    vector<dabSubchannel*>::iterator subchannel = ensemble->subchannels.end();
    vector<dabSubchannel*>::iterator subchannelIndicator;
    vector<dabOutput*>::iterator output;
    dabProtection* protection = NULL;

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
    unsigned timestamp = 0xffffff;


    char* progName = strrchr(argv[0], '/');
    if (progName == NULL) {
        progName = argv[0];
    } else {
        ++progName;
    }


    while (1) {
        int c = getopt(argc, argv,
                "A:B:CD:E:F:L:M:O:P:STVa:b:c:de:f:g:hi:kl:m:n:op:st:y:z");
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
                returnCode = -1;
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
            currentFrame = 0;    // Will be used temporaly for SCIdS

            break;
        case 'C':
            if (service == ensemble->services.end()) {
                etiLog.printHeader(TcpLog::ERR, "You must define a service"
                        " before using option -%c\n", c);
                printUsage(progName);
                returnCode = -1;
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
            (*component)->SCIdS = currentFrame++;
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
                returnCode = -1;
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
                    returnCode = -1;
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
                    returnCode = -1;
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
                    returnCode = -1;
                    goto EXIT;
                }

                (*subchannel)->type = 1;
                (*subchannel)->bitrate = DEFAULT_DATA_BITRATE;
#endif
            } else {
                etiLog.printHeader(TcpLog::ERR,
                        "Service '%c' not yet coded!\n", c);
                returnCode = -1;
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
                returnCode = -1;
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
                returnCode = -1;
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
                        returnCode = -1;
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
                    returnCode = -1;
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
                        returnCode = -1;
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
                    returnCode = -1;
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
                        returnCode = -1;
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
                    returnCode = -1;
                    goto EXIT;
                }
            }
            break;
        case 'i':
            if (optarg == NULL) {
                etiLog.printHeader(TcpLog::ERR,
                        "Missing parameter for option -i\n");
                printUsage(progName);
                returnCode = -1;
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
                    returnCode = -1;
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
                returnCode = -1;
                goto EXIT;
            }
            if (subchannel == ensemble->subchannels.end()) {
                etiLog.printHeader(TcpLog::ERR,
                        "You must define a subchannel first!\n");
                printUsage(progName);
                returnCode = -1;
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
                returnCode = -1;
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
                returnCode = -1;
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
                    returnCode = -1;
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
                    returnCode = -1;
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
                returnCode = -1;
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
                returnCode = -1;
                goto EXIT;
            }
            if (subchannel == ensemble->subchannels.end()) {
                etiLog.printHeader(TcpLog::ERR,
                        "You must define a subchannel first!\n");
                printUsage(progName);
                returnCode = -1;
                goto EXIT;
            }
            level = strtoul(optarg, NULL, 0) - 1;
            if (protection->form == 0) {
                if ((level < 0) || (level > 4)) {
                    etiLog.printHeader(TcpLog::ERR,
                            "protection level must be between "
                            "1 to 5 inclusively (current = %i)\n", level);
                    returnCode = -1;
                    goto EXIT;
                }
            } else {
                if ((level < 0) || (level > 3)) {
                    etiLog.printHeader(TcpLog::ERR,
                            "protection level must be between "
                            "1 to 4 inclusively (current = %i)\n", level);
                    returnCode = -1;
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
                    returnCode = -1;
                    goto EXIT;
                }
                if (ensemble->mode == 4)
                    ensemble->mode = 0;
                if  (ensemble->mode == 3) {
                    FICL = 32;
                } else {
                    FICL = 24;
                }
            } else {
                etiLog.printHeader(TcpLog::ERR,
                        "Missing parameter for option -m\n");
                printUsage(progName);
                returnCode = -1;
                goto EXIT;
            }
            break;
        case 'n':
            if (optarg) {
                limit = strtoul(optarg, NULL, 0);
            } else {
                etiLog.printHeader(TcpLog::ERR,
                        "Missing parameter for option -n\n");
                printUsage(progName);
                returnCode = -1;
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
                returnCode = -1;
                goto EXIT;
            }
            if (component == ensemble->components.end()) {
                etiLog.printHeader(TcpLog::ERR,
                        "You must define a component before setting "
                        "service type!\n");
                printUsage(progName);
                returnCode = -1;
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
                returnCode = -1;
                goto EXIT;
            }
            if (!(*component)->isPacketComponent(ensemble->subchannels)) {
                etiLog.printHeader(TcpLog::ERR, "address\n");
                printUsage(progName);
                returnCode = -1;
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
                returnCode = -1;
                goto EXIT;
            }
            if (!(*component)->isPacketComponent(ensemble->subchannels)) {
                etiLog.printHeader(TcpLog::ERR, "datagroup\n");
                printUsage(progName);
                returnCode = -1;
                goto EXIT;
            }
            (*component)->packet.datagroup = true;
            break;
        case 'f':
            if (component == ensemble->components.end()) {
                etiLog.printHeader(TcpLog::ERR,
                        "You must define a component first!\n");
                printUsage(progName);
                returnCode = -1;
                goto EXIT;
            }
            if (!(*component)->isPacketComponent(ensemble->subchannels)) {
                etiLog.printHeader(TcpLog::ERR, "application type\n");
                printUsage(progName);
                returnCode = -1;
                goto EXIT;
            }
            (*component)->packet.appType = strtoul(optarg, NULL, 0);
            break;
        case 'g':
            if (service == ensemble->services.end()) {
                etiLog.printHeader(TcpLog::ERR, "You must define a service"
                        " before using option -%c\n", c);
                printUsage(progName);
                returnCode = -1;
                goto EXIT;
            }
            (*service)->language = strtoul(optarg, NULL, 0);
            break;
        case 's':
            {
                struct timeval tv;
                gettimeofday(&tv, NULL);
                unsigned _8ms = (tv.tv_usec / 1000) / 8;
                unsigned _1ms = (tv.tv_usec - (_8ms * 8000)) / 1000;
                unsigned _4us = 20;
                unsigned _488ns = 0;
                unsigned _61ns = 0;
                timestamp = (((((((_8ms << 3) | _1ms) << 8) | _4us) << 3) | _488ns) << 8) | _61ns;
            }
            break;
        case 'y':
            if (service == ensemble->services.end()) {
                etiLog.printHeader(TcpLog::ERR, "You must define a service"
                        " before using option -%c\n", c);
                printUsage(progName);
                returnCode = -1;
                goto EXIT;
            }
            (*service)->pty = strtoul(optarg, NULL, 0);
            break;
        case 'z':
            factumAnalyzer = true;
            break;
        case '?':
            returnCode = -1;
        case 'h':
            printUsage(progName, stdout);
            goto EXIT;
        default:
            etiLog.printHeader(TcpLog::ERR, "Option '%c' not coded yet\n", c);
            returnCode = -1;
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
        returnCode = -1;
        goto EXIT;
    }

    if (outputs.size() == 0) {
        etiLog.printHeader(TcpLog::WARNING, "no output defined\n");

        outputs.push_back(new dabOutput);
        output = outputs.end() - 1;

        memset(*output, 0, sizeof(dabOutput));
        (*output)->outputProto = "simul";
        (*output)->outputName = NULL;
        (*output)->data = NULL;
        (*output)->operations = dabOutputDefaultOperations;

        printf("Press enter to continue\n");
        getchar();
    }

    // Check and adjust subchannels
    {
        set<unsigned char> ids;
        for (subchannel = ensemble->subchannels.begin();
                subchannel != ensemble->subchannels.end();
                ++subchannel) {
            if (ids.find((*subchannel)->id) != ids.end()) {
                etiLog.printHeader(TcpLog::ERR,
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
                etiLog.printHeader(TcpLog::ERR,
                        "Service id 0x%x (%u) is set more than once!\n",
                        (*service)->id, (*service)->id);
                returnCode = -1;
                goto EXIT;
            }

            // Get first component of this service
            component = getComponent(ensemble->components, (*service)->id);
            if (component == ensemble->components.end()) {
                etiLog.printHeader(TcpLog::ERR,
                        "Service %u includes no component!\n");
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
                etiLog.printHeader(TcpLog::ERR,
                        "Error, unknown service type: %u\n", (*service)->getType(ensemble));
                returnCode = -1;
                goto EXIT;
            }

            // Adjust components type for DAB+
            while (component != ensemble->components.end()) {
                subchannel =
                    getSubchannel(ensemble->subchannels, (*component)->subchId);
                if (subchannel == ensemble->subchannels.end()) {
                    etiLog.printHeader(TcpLog::ERR, "Error, service %u component "
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
                    etiLog.printHeader(TcpLog::ERR,
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
        if (strcmp((*output)->outputProto, "file") == 0) {
            (*output)->operations = dabOutputFileOperations;
#else // !defined(HAVE_OUTPUT_FILE)
        if (0) {
#endif // defined(HAVE_OUTPUT_FILE)
#if defined(HAVE_OUTPUT_FIFO)
        } else if (strcmp((*output)->outputProto, "fifo") == 0) {
            (*output)->operations = dabOutputFifoOperations;
#endif // !defined(HAVE_OUTPUT_FIFO)
#if defined(HAVE_OUTPUT_RAW)
        } else if (strcmp((*output)->outputProto, "raw") == 0) {
            (*output)->operations = dabOutputRawOperations;
#endif // defined(HAVE_OUTPUT_RAW)
#if defined(HAVE_OUTPUT_UDP)
        } else if (strcmp((*output)->outputProto, "udp") == 0) {
            (*output)->operations = dabOutputUdpOperations;
#endif // defined(HAVE_OUTPUT_UDP)
#if defined(HAVE_OUTPUT_TCP)
        } else if (strcmp((*output)->outputProto, "tcp") == 0) {
            (*output)->operations = dabOutputTcpOperations;
#endif // defined(HAVE_OUTPUT_TCP)
#if defined(HAVE_OUTPUT_SIMUL)
        } else if (strcmp((*output)->outputProto, "simul") == 0) {
            (*output)->operations = dabOutputSimulOperations;
#endif // defined(HAVE_OUTPUT_SIMUL)
        } else {
            etiLog.printHeader(TcpLog::ERR, "Output protcol unknown: %s\n",
                    (*output)->outputProto);
            goto EXIT;
        }

        if ((*output)->operations.init(&(*output)->data) == -1) {
            etiLog.printHeader(TcpLog::ERR, "Unable to init output %s://%s\n",
                    (*output)->outputProto, (*output)->outputName);
            return -1;
        }
        if ((*output)->operations.open((*output)->data, (*output)->outputName)
                == -1) {
            etiLog.printHeader(TcpLog::ERR, "Unable to open output %s://%s\n",
                    (*output)->outputProto, (*output)->outputName);
            return -1;
        }
    }

    //Relatif aux fichiers d'entre
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
        if ((*subchannel)->operations.open((*subchannel)->data,
                    (*subchannel)->inputName) == -1) {
            perror((*subchannel)->inputName);
            returnCode = -1;
            goto EXIT;
        }

        // TODO Check errors
        int result = (*subchannel)->operations.setBitrate(
                &(*subchannel)->operations, (*subchannel)->data,
                (*subchannel)->bitrate);
        if (result <= 0) {
            etiLog.printHeader(TcpLog::ERR, "can't set bitrate for source %s\n",
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
        etiLog.printHeader(TcpLog::ERR, "can't multiplexed no subchannel!\n");
        returnCode = -1;
        goto EXIT;
    }

    subchannel = ensemble->subchannels.end() - 1;
    if ((*subchannel)->startAddress + getSizeCu((*subchannel)) > 864) {
        etiLog.printHeader(TcpLog::ERR, "Total size in CU exceeds 864\n");
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
            etiLog.printHeader(TcpLog::ERR,
                    "Subchannel %i does not exist for component "
                    "of service %i\n",
                    (*component)->subchId, (*component)->serviceId);
            returnCode = -1;
            goto EXIT;
        }
        if ((*subchannel)->type != 3) continue;

        (*component)->packet.id = cur++;
    }

    //Initialisation a 0 des cases de la trame ETI
    memset(etiFrame, 0, 6144);

    // Print settings before starting
    etiLog.printHeader(TcpLog::INFO, "\n--- Multiplex configuration ---\n");
    printEnsemble(ensemble);

    etiLog.printHeader(TcpLog::INFO, "\n--- Subchannels list ---\n");
    printSubchannels(ensemble->subchannels);

    etiLog.printHeader(TcpLog::INFO, "\n--- Services list ---\n");
    printServices(ensemble->services);

    etiLog.printHeader(TcpLog::INFO, "\n--- Components list ---\n");
    printComponents(ensemble->components);

    etiLog.printHeader(TcpLog::INFO, "\n--- Output list ---\n");
    printOutputs(outputs);

    etiLog.printHeader(TcpLog::INFO, "\n");


    /***************************************************************************
     **********   Boucle principale, chaque passage cree une trame  ************
     **************************************************************************/
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

        //Initialisation a 0 des cases de la trame
        memset(etiFrame, 0, 6144);

        /**********************************************************************
         **********   Section SYNC du ETI(NI, G703)   *************************
         **********************************************************************/

        //declare une instance d'une structure eti_SYNC
        eti_SYNC *etiSync = (eti_SYNC *) etiFrame;    
        //****** Section ERR ******//
        // 1 octet
        etiSync->ERR = 0xFF;    //Indique qu'il n'y a pas d'erreur
        //****** Section FSYNC *****//
        // 3 octets, pour la synchronisation, alterne a chaque trame entre les
        // deux symboles
        sync ^= 0xffffff;
        etiSync->FSYNC = sync;

        /**********************************************************************
         ***********   Section LIDATA du ETI(NI, G703)   **********************
         **********************************************************************/

        //****** Section FC ***************************************************/
        // 4 octets
        // declare une instance d une structure eti_FC et la place dans la trame
        eti_FC *fc = (eti_FC *) & etiFrame[4];

        //****** FCT **********************//
        //Incremente a chaque trame, de 0 a 249, 1 octet
        fc->FCT = currentFrame % 250;

        //****** FICF ******//
        //Fast Information Channel Flag, 1 bit, =1 si le FIC est present
        fc->FICF = 1;

        //****** NST ******//
        //Number of Stream, 7 bits,  0-64, 0 si reconfiguration du multiplex
        fc->NST = ensemble->subchannels.size();

        //****** FP ******//
        // Frame Phase, 3 bits, compteur sur 3 bits, permet au COFDM generator
        // de savoir quand inserer le TII utilise egalement par le MNST
        fc->FP = currentFrame & 0x7;

        //****** MID ******//
        //Mode Identity, 2 bits, 01 ModeI, 10 modeII, 11 ModeIII, 00 ModeIV
        fc->MID = ensemble->mode;      //mode 2 demande 3 FIB, 3*32octets = 96octets

        //****** FL ******//
        //Frame Length, 11 bits, nb of words(4 bytes) in STC, EOH and MST  
        // si NST=0, FL=1+FICL words, FICL=24 ou 32 selon le mode
        //en word ( 4 bytes), +1 pour la partie EOH
        FLtmp = 1 + FICL + (fc->NST);

        for (subchannel = ensemble->subchannels.begin();
                subchannel != ensemble->subchannels.end();
                ++subchannel) {
            //Taille d'une trame audio mp2 en paquet de 64 bits
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
                        etiLog.printHeader(TcpLog::ERR,
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
                        etiLog.printHeader(TcpLog::ERR,
                                "Component type not supported\n");
                        returnCode = -1;
                        goto EXIT;
                    }
                    index += 2;
                    fig0_2->Length += 2;
                    figSize += 2;
                    if (figSize > 30) {
                        etiLog.printHeader(TcpLog::ERR,
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
                        etiLog.printHeader(TcpLog::ERR,
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
                        etiLog.printHeader(TcpLog::ERR,
                                "Component type not supported\n");
                        returnCode = -1;
                        goto EXIT;
                    }
                    index += 2;
                    fig0_2->Length += 2;
                    figSize += 2;
                    if (figSize > 30) {
                        etiLog.printHeader(TcpLog::ERR,
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
                    etiLog.printHeader(TcpLog::ERR,
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
                    etiLog.printHeader(TcpLog::ERR,
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
            etiLog.printHeader(TcpLog::ERR,
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
                    etiLog.printHeader(TcpLog::ERR,
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
                    etiLog.printHeader(TcpLog::ERR,
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

            memcpy(&etiFrame[index], ensemble->label.text, 16);
            index = index + 16;

            etiFrame[index++] = ((char *) &ensemble->label.flag)[1];
            etiFrame[index++] = ((char *) &ensemble->label.flag)[0];

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
                    etiLog.printHeader(TcpLog::ERR,
                            "Subchannel %i does not exist for component "
                            "of service %i\n",
                            (*component)->subchId, (*component)->serviceId);
                    returnCode = -1;
                    goto EXIT;
                }

                if ((*subchannel)->type != 3) continue;

                if ((*component)->packet.appType != 0xffff) {
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
                        etiLog.printHeader(TcpLog::ERR,
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

            // FIG type 1/1, SI, Service label, une instance par sous-canal
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
            memcpy(&etiFrame[index], (*service)->label.text, 16);
            index += 16;
            figSize += 16;

            etiFrame[index++] = ((char *) &(*service)->label.flag)[1];
            etiFrame[index++] = ((char *) &(*service)->label.flag)[0];
            figSize += 2;
        } else if (alterneFIB <
                ensemble->services.size() + ensemble->components.size()) {
            component = ensemble->components.begin() +
                (alterneFIB - ensemble->services.size());
            service = getService(*component, ensemble->services);
            subchannel =
                getSubchannel(ensemble->subchannels, (*component)->subchId);

            if ((*component)->label.text[0] != 0) {
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
                memcpy(&etiFrame[index], (*component)->label.text, 16);
                index += 16;
                figSize += 16;

                etiFrame[index++] = ((char *) &(*component)->label.flag)[1];
                etiFrame[index++] = ((char *) &(*component)->label.flag)[0];
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
            etiLog.printHeader(TcpLog::ERR,
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
            if ((*subchannel)->operations.lock != NULL) {
                (*subchannel)->operations.lock((*subchannel)->data);
            }
            result = (*subchannel)->operations.readFrame(
                    &(*subchannel)->operations,
                    (*subchannel)->data, &etiFrame[index], sizeSubchannel);
            if ((*subchannel)->operations.unlock != NULL) {
                (*subchannel)->operations.unlock((*subchannel)->data);
            }
            if (result < 0) {
                etiLog.print(TcpLog::INFO, "ETI frame number: %i\n", currentFrame);
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

        tist->TIST = htonl(timestamp) | 0xff;
        if (timestamp != 0xffffff) {
            timestamp += 3 << 17;
            if (timestamp > 0xf9ffff) {
                timestamp -= 0xfa0000;
            }
        }

        /**********************************************************************
         ***********   Section FRPD   *****************************************
         **********************************************************************/

        //Donne le nombre total d'octets utils dans la trame
        index = (FLtmp + 1 + 1 + 1 + 1) * 4;

        for (output = outputs.begin() ; output != outputs.end(); ++output) {
            if ((*output)->operations.write((*output)->data, etiFrame, index)
                    == -1) {
                etiLog.print(TcpLog::ERR, "Can't write to output %s://%s\n",
                        (*output)->outputProto, (*output)->outputName);
            }
        }

#ifdef DUMP_BRIDGE
        dumpBytes(dumpData, sizeSubChannel, stderr);
#endif // DUMP_BRIDGE

        if (currentFrame % 100 == 0) {
            etiLog.print(TcpLog::INFO, "ETI frame number %i \n", currentFrame);
        }
    }

EXIT:
    etiLog.print(TcpLog::DBG, "exiting...\n");
    //fermeture des fichiers
    for (subchannel = ensemble->subchannels.begin();
            subchannel != ensemble->subchannels.end();
            ++subchannel) {
        (*subchannel)->operations.close((*subchannel)->data);
        (*subchannel)->operations.clean(&(*subchannel)->data);
    }
    for (output = outputs.begin() ; output != outputs.end(); ++output) {
        (*output)->operations.close((*output)->data);
        (*output)->operations.clean(&(*output)->data);
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
        etiLog.print(TcpLog::EMERG, "...aborting\n");
    } else {
        etiLog.print(TcpLog::DBG, "...done\n");
    }

    return returnCode;
}
