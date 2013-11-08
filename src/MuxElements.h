/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Includes modifications
   2012, Matthias P. Braendli, matthias.braendli@mpb.li

   This file defines all data structures used in DabMux to represent
   and save ensemble data.
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
#ifndef _MUX_ELEMENTS
#define _MUX_ELEMENTS

#include <vector>
#include <functional>
#include <algorithm>
#include <stdint.h>
#include "dabOutput/dabOutput.h"
#include "dabInput.h"
#include "Eti.h"

using namespace std;

struct dabOutput {
    dabOutput(const char* proto, const char* name) :
        outputProto(proto), outputName(name), output(NULL) { }

    // outputs are specified with outputProto://outputName
    // during config parsing
    std::string outputProto;
    std::string outputName;

    // later, the corresponding output is then created
    DabOutput* output;
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

    bool isPacketComponent(vector<dabSubchannel*>& subchannels);
};



struct dabService {
    dabLabel label;
    uint32_t id;
    unsigned char pty;
    unsigned char language;
    bool program;

    unsigned char getType(dabEnsemble* ensemble);
    unsigned char nbComponent(vector<dabComponent*>& components);
};

vector<dabSubchannel*>::iterator getSubchannel(
        vector<dabSubchannel*>& subchannels, int id);

vector<dabComponent*>::iterator getComponent(
        vector<dabComponent*>& components,
        uint32_t serviceId,
        vector<dabComponent*>::iterator current);

vector<dabComponent*>::iterator getComponent(
        vector<dabComponent*>& components,
        uint32_t serviceId);

vector<dabService*>::iterator getService(
        dabComponent* component,
        vector<dabService*>& services);

unsigned short getSizeCu(dabSubchannel* subchannel);

unsigned short getSizeDWord(dabSubchannel* subchannel);

unsigned short getSizeByte(dabSubchannel* subchannel);

unsigned short getSizeWord(dabSubchannel* subchannel);


#endif
