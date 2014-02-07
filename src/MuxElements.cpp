/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Includes modifications
   2012, Matthias P. Braendli, matthias.braendli@mpb.li
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

#include <vector>

#include "MuxElements.h"

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



using namespace std;

vector<dabSubchannel*>::iterator getSubchannel(
        vector<dabSubchannel*>& subchannels, int id)
{
    return find_if(
            subchannels.begin(),
            subchannels.end(),
            bind2nd(SubchannelId(), id)
            );
}

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

bool dabComponent::isPacketComponent(vector<dabSubchannel*>& subchannels)
{
    if (subchId > 63) {
        etiLog.log(error,
                "You must define subchannel id in the "
                "packet component before defining packet ");
        return false;
    }
    if (getSubchannel(subchannels, subchId) == subchannels.end()) {
        etiLog.log(error,
                "Invalid subchannel id in the packet component "
                "for defining packet ");
        return false;
    }
    if ((*getSubchannel(subchannels, subchId))->type != 3) {
        etiLog.log(error,
                "Invalid component type for defining packet ");
        return false;
    }
    return true;
}


unsigned char dabService::getType(dabEnsemble* ensemble)
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

unsigned char dabService::nbComponent(vector<dabComponent*>& components)
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
                etiLog.log(error, "Bad protection level on "
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
                etiLog.log(error,
                        "Bad protection level on subchannel\n");
                return 0;
            }
            break;
        default:
            etiLog.log(error, "Invalid protection option\n");
            return 0;
        }
    }
    return 0;
}

unsigned short getSizeDWord(dabSubchannel* subchannel)
{
    return (subchannel->bitrate * 3) >> 3;
}

unsigned short getSizeByte(dabSubchannel* subchannel)
{
    return subchannel->bitrate * 3;
}


unsigned short getSizeWord(dabSubchannel* subchannel)
{
    return (subchannel->bitrate * 3) >> 2;
}


