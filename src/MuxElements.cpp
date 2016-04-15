/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2014, 2015
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

#include <vector>
#include <memory>
#include <algorithm>

#include "MuxElements.h"
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>

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

std::string AnnouncementCluster::tostring() const
{
    stringstream ss;
    ss << "cluster id(" << (int)cluster_id;
    ss << ", flags 0x" << boost::format("%04x") % flags;
    ss << ", subchannel " << subchanneluid;
    if (m_active) {
        ss << " *";
    }

    ss << " )";

    return ss.str();
}

void AnnouncementCluster::set_parameter(const string& parameter,
        const string& value)
{
    if (parameter == "active") {
        stringstream ss;
        ss << value;
        ss >> m_active;
    }
    else {
        stringstream ss;
        ss << "Parameter '" << parameter <<
            "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss.str());
    }
}

const string AnnouncementCluster::get_parameter(const string& parameter) const
{
    stringstream ss;
    if (parameter == "active") {
        ss << m_active;
    }
    else {
        ss << "Parameter '" << parameter <<
            "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss.str());
    }
    return ss.str();
}


int DabLabel::setLabel(const std::string& label)
{
    size_t len = label.length();
    if (len > DABLABEL_LENGTH)
        return -3;

    m_flag = 0xFF00; // truncate the label to the eight first characters

    m_label = label;

    return 0;
}

int DabLabel::setLabel(const std::string& label, const std::string& short_label)
{
    DabLabel newlabel;

    int result = newlabel.setLabel(label);
    if (result < 0)
        return result;

    /* First check if we can actually create the short label */
    int flag = newlabel.setShortLabel(short_label);
    if (flag < 0)
        return flag;

    // short label is valid.
    m_flag = flag & 0xFFFF;
    m_label = newlabel.m_label;

    return 0;
}

/* The label.flag is a 16bit mask that tells which label
 * characters are to be used for the short label
 *
 * From EN 300 401, clause 5.2.2.2.1:
 *
 * Character flag field: this 16-bit flag field shall indicate which of the
 * characters of the character field are to be
 * displayed in an abbreviated form of the label, as follows:
 *  bi: (i = 0, ... ,15);
 *  0: not to be displayed in abbreviated label;
 *  1: to be displayed in abbreviated label.
 * NOTE: Not more than 8 of the bi may be set to "1".
 *
 * returns:  the flag (16 bits) on success
 *          -1 if the short_label is not a representable
 *          -2 if the short_label is too long
 */
int DabLabel::setShortLabel(const std::string& slabel)
{
    const char* slab = slabel.c_str();
    uint16_t flag = 0x0;

    /* Iterate over the label and set the bits in the flag
     * according to the characters in the slabel
     */
    for (size_t i = 0; i < m_label.size(); ++i) {
        if (*slab == m_label[i]) {
            flag |= 0x8000 >> i;
            if (*(++slab) == '\0') {
                break;
            }
        }
    }

    /* If we have remaining characters in the slabel after
     * we went through the whole label, the short label
     * cannot be represented
     */
    if (*slab != '\0') {
        return -1;
    }

    /* Count the number of bits in the flag */
    int count = 0;
    for (int i = 0; i < 16; ++i) {
        if (flag & (1 << i)) {
            ++count;
        }
    }
    if (count > 8) {
        return -2;
    }

    return flag;
}

const string DabLabel::short_label() const
{
    stringstream shortlabel;
    for (size_t i = 0; i < m_label.size(); ++i) {
        if (m_flag & 0x8000 >> i) {
            shortlabel << m_label[i];
        }
    }

    return shortlabel.str();
}

void DabLabel::writeLabel(uint8_t* buf) const
{
    memset(buf, ' ', DABLABEL_LENGTH);
    if (m_label.size() <= DABLABEL_LENGTH) {
        std::copy(m_label.begin(), m_label.end(), (char*)buf);
    }
}

vector<dabSubchannel*>::iterator getSubchannel(
        vector<dabSubchannel*>& subchannels, int id)
{
    return find_if(
            subchannels.begin(),
            subchannels.end(),
            bind2nd(SubchannelId(), id)
            );
}

vector<DabComponent*>::iterator getComponent(
        vector<DabComponent*>& components,
        uint32_t serviceId,
        vector<DabComponent*>::iterator current)
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


vector<DabComponent*>::iterator getComponent(
        vector<DabComponent*>& components,
        uint32_t serviceId) {
    return getComponent(components, serviceId, components.end());
}

std::vector<std::shared_ptr<DabService> >::iterator getService(
        DabComponent* component,
        std::vector<std::shared_ptr<DabService> >& services)
{
    size_t i = 0;
    for (auto service : services) {
        if (service->id == component->serviceId) {
            return services.begin() + i;
        }
        i++;
    }

    throw std::runtime_error("Service not included in any component");
}

bool DabComponent::isPacketComponent(vector<dabSubchannel*>& subchannels) const
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
    if ((*getSubchannel(subchannels, subchId))->type != subchannel_type_t::Packet) {
        return false;
    }
    return true;
}

void DabComponent::set_parameter(const string& parameter,
        const string& value)
{
    if (parameter == "label") {
        vector<string> fields;
        boost::split(fields, value, boost::is_any_of(","));
        if (fields.size() != 2) {
            throw ParameterError("Parameter 'label' must have format"
                    " 'label,shortlabel'");
        }
        int success = this->label.setLabel(fields[0], fields[1]);
        stringstream ss;
        switch (success)
        {
            case 0:
                break;
            case -1:
                ss << m_name << " short label " <<
                    fields[1] << " is not subset of label '" <<
                    fields[0] << "'";
                etiLog.level(warn) << ss.str();
                throw ParameterError(ss.str());
            case -2:
                ss << m_name << " short label " <<
                    fields[1] << " is too long (max 8 characters)";
                etiLog.level(warn) << ss.str();
                throw ParameterError(ss.str());
            case -3:
                ss << m_name << " label " <<
                    fields[0] << " is too long (max 16 characters)";
                etiLog.level(warn) << ss.str();
                throw ParameterError(ss.str());
            default:
                ss << m_name << " short label definition: program error !";
                etiLog.level(emerg) << ss.str();
                throw ParameterError(ss.str());
        }
    }
    else {
        stringstream ss;
        ss << "Parameter '" << parameter <<
            "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss.str());
    }
}

const string DabComponent::get_parameter(const string& parameter) const
{
    stringstream ss;
    if (parameter == "label") {
        ss << label.long_label() << "," << label.short_label();
    }
    else {
        ss << "Parameter '" << parameter <<
            "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss.str());
    }
    return ss.str();

}

subchannel_type_t DabService::getType(std::shared_ptr<dabEnsemble> ensemble) const
{
    vector<dabSubchannel*>::iterator subchannel;
    vector<DabComponent*>::iterator component =
        getComponent(ensemble->components, id);
    if (component == ensemble->components.end()) {
        throw std::runtime_error("No component found for service");
    }

    subchannel = getSubchannel(ensemble->subchannels, (*component)->subchId);
    if (subchannel == ensemble->subchannels.end()) {
        throw std::runtime_error("Could not find subchannel associated with service");
    }

    return (*subchannel)->type;
}

bool DabService::isProgramme(std::shared_ptr<dabEnsemble> ensemble) const
{
    bool ret = false;
    switch (getType(ensemble)) {
        case subchannel_type_t::Audio: // Audio
            ret = true;
            break;
        case subchannel_type_t::DataDmb:
        case subchannel_type_t::Fidc:
        case subchannel_type_t::Packet:
            ret = false;
            break;
        default:
            etiLog.log(error,
                    "Error, unknown service type: %u\n",
                    getType(ensemble));
            throw std::runtime_error("DabService::isProgramme unknown service type");
    }

    return ret;
}


unsigned char DabService::nbComponent(vector<DabComponent*>& components) const
{
    int nb = 0;
    vector<DabComponent*>::iterator current;

    for (current = components.begin(); current != components.end();
            ++current) {
        if ((*current)->serviceId == id) {
            ++nb;
        }
    }
    return nb;
}

void DabService::set_parameter(const string& parameter,
        const string& value)
{
    if (parameter == "label") {
        vector<string> fields;
        boost::split(fields, value, boost::is_any_of(","));
        if (fields.size() != 2) {
            throw ParameterError("Parameter 'label' must have format"
                   " 'label,shortlabel'");
        }
        int success = this->label.setLabel(fields[0], fields[1]);
        stringstream ss;
        switch (success)
        {
            case 0:
                break;
            case -1:
                ss << m_name << " short label " <<
                    fields[1] << " is not subset of label '" <<
                    fields[0] << "'";
                etiLog.level(warn) << ss.str();
                throw ParameterError(ss.str());
            case -2:
                ss << m_name << " short label " <<
                    fields[1] << " is too long (max 8 characters)";
                etiLog.level(warn) << ss.str();
                throw ParameterError(ss.str());
            case -3:
                ss << m_name << " label " <<
                    fields[0] << " is too long (max 16 characters)";
                etiLog.level(warn) << ss.str();
                throw ParameterError(ss.str());
            default:
                ss << m_name << " short label definition: program error !";
                etiLog.level(emerg) << ss.str();
                throw ParameterError(ss.str());
        }
    }
    else if (parameter == "pty") {
        int newpty = std::stoi(value); // International code, 5 bits

        if (newpty >= 0 and newpty < (1<<5)) {
            pty = newpty;
        }
        else {
            stringstream ss;
            ss << m_name << " pty is out of bounds";
            etiLog.level(warn) << ss.str();
            throw ParameterError(ss.str());
        }
    }
    else {
        stringstream ss;
        ss << "Parameter '" << parameter <<
            "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss.str());
    }
}

const string DabService::get_parameter(const string& parameter) const
{
    stringstream ss;
    if (parameter == "label") {
        ss << label.long_label() << "," << label.short_label();
    }
    else if (parameter == "pty") {
        ss << (int)pty;
    }
    else {
        ss << "Parameter '" << parameter <<
            "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss.str());
    }
    return ss.str();

}

void dabEnsemble::set_parameter(const string& parameter, const string& value)
{
    if (parameter == "localtimeoffset") {
        if (value == "auto") {
            lto_auto = true;
        }
        else {
            lto_auto = false;
            int new_lto = atol(value.c_str());

            if (new_lto < -24) {
                throw ParameterError("Desired local time offset too small."
                        " Minimum -24" );
            }
            else if (new_lto > 24) {
                throw ParameterError("Desired local time offset too large."
                        " Maximum 24" );
            }

            this->lto = new_lto;
        }
    }
    else {
        stringstream ss;
        ss << "Parameter '" << parameter <<
            "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss.str());
    }
}

const string dabEnsemble::get_parameter(const string& parameter) const
{
    stringstream ss;
    if (parameter == "localtimeoffset") {
        if (this->lto_auto) {
            ss << "auto(" << this->lto << ")";
        }
        else {
            ss << this->lto;
        }
    }
    else {
        ss << "Parameter '" << parameter <<
            "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss.str());
    }
    return ss.str();
}

unsigned short getSizeCu(dabSubchannel* subchannel)
{
    if (subchannel->protection.form == UEP) {
        return Sub_Channel_SizeTable[subchannel->
            protection.uep.tableIndex];
    }
    else if (subchannel->protection.form == EEP) {
        dabProtectionEEP* protection =
            &subchannel->protection.eep;
        switch (protection->profile) {
        case EEP_A:
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
        case EEP_B:
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


