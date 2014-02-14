/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2014
   Matthias P. Braendli, matthias.braendli@mpb.li

   This file defines all data structures used in DabMux to represent
   and save ensemble data.
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
#ifndef _MUX_ELEMENTS
#define _MUX_ELEMENTS

#include <vector>
#include <string>
#include <functional>
#include <algorithm>
#include <stdint.h>
#include "dabOutput/dabOutput.h"
#include "dabInput.h"
#include "RemoteControl.h"
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


class DabLabel
{
    public:
        /* Set a new label and short label.
         * returns:  0 on success
         *          -1 if the short_label is not a representable
         *          -2 if the short_label is too long
         *          -3 if the text is too long
         */
        int setLabel(const std::string& text, const std::string& short_label);

        /* Same as above, but sets the flag to 0xff00, truncating at 8
         * characters.
         *
         * returns:  0 on success
         *          -3 if the text is too long
         */
        int setLabel(const std::string& text);

        const char* text() const { return m_text; }
        uint16_t flag() const { return m_flag; }
        const string short_label() const;

    private:
        // In the DAB standard, the label is 16 chars.
        // We keep it here zero-terminated
        char m_text[17];
        uint16_t m_flag;
        int setShortLabel(const std::string& slabel);
};


class DabService;
class DabComponent;

struct dabSubchannel;
struct dabEnsemble {
    uint16_t id;
    uint8_t ecc;
    DabLabel label;
    uint8_t mode;
    vector<DabService*> services;
    vector<DabComponent*> components;
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
    DabInputBase* input;
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


class DabComponent : public RemoteControllable
{
    public:
        DabComponent(std::string uid) :
            RemoteControllable(uid)
        {
            RC_ADD_PARAMETER(label, "Label and shortlabel [label,short]");
        }

        DabLabel label;
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

        /* Remote control */
        virtual void set_parameter(string parameter, string value);

        /* Getting a parameter always returns a string. */
        virtual string get_parameter(string parameter);


    private:
        const DabComponent& operator=(const DabComponent& other);
        DabComponent(const DabComponent& other);
};



class DabService : public RemoteControllable
{
    public:
        DabService(std::string uid) :
            RemoteControllable(uid)
        {
            RC_ADD_PARAMETER(label, "Label and shortlabel [label,short]");
        }

        uint32_t id;
        unsigned char pty;
        unsigned char language;
        bool program;

        unsigned char getType(dabEnsemble* ensemble);
        unsigned char nbComponent(vector<DabComponent*>& components);

        DabLabel label;

        /* Remote control */
        virtual void set_parameter(string parameter, string value);

        /* Getting a parameter always returns a string. */
        virtual string get_parameter(string parameter);

    private:
        const DabService& operator=(const DabService& other);
        DabService(const DabService& other);
};

vector<dabSubchannel*>::iterator getSubchannel(
        vector<dabSubchannel*>& subchannels, int id);

vector<DabComponent*>::iterator getComponent(
        vector<DabComponent*>& components,
        uint32_t serviceId,
        vector<DabComponent*>::iterator current);

vector<DabComponent*>::iterator getComponent(
        vector<DabComponent*>& components,
        uint32_t serviceId);

vector<DabService*>::iterator getService(
        DabComponent* component,
        vector<DabService*>& services);

unsigned short getSizeCu(dabSubchannel* subchannel);

unsigned short getSizeDWord(dabSubchannel* subchannel);

unsigned short getSizeByte(dabSubchannel* subchannel);

unsigned short getSizeWord(dabSubchannel* subchannel);


#endif
