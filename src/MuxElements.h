/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2018
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://www.opendigitalradio.org

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
#pragma once

#include <vector>
#include <memory>
#include <mutex>
#include <string>
#include <functional>
#include <exception>
#include <algorithm>
#include <chrono>
#include <boost/optional.hpp>
#include <stdint.h>
#include "dabOutput/dabOutput.h"
#include "input/inputs.h"
#include "RemoteControl.h"
#include "Eti.h"

class MuxInitException : public std::exception
{
    public:
        MuxInitException(const std::string m = "ODR-DabMux initialisation error")
            throw()
            : msg(m) {}
        ~MuxInitException(void) throw() {}
        const char* what() const throw() { return msg.c_str(); }
    private:
        std::string msg;
};


enum class subchannel_type_t {
    Audio = 0,
    DataDmb = 1,
    Fidc = 2,
    Packet = 3
};


/* Announcement switching flags,
 * define in ETSI TS 101 756 Table 15 */
const char * const annoucement_flags_names[] = {
    "Alarm",
    "Traffic",
    "Travel",
    "Warning",
    "News",
    "Weather",
    "Event",
    "Special",
    "ProgrammeInfo",
    "Sports",
    "Finance",
    "tba1", "tba2", "tba3", "tba4", "tba5"
};

/* Class representing an announcement cluster for FIG 0/19 */
class AnnouncementCluster : public RemoteControllable {
    public:
        AnnouncementCluster(const std::string& name) :
            RemoteControllable(name)
        {
            RC_ADD_PARAMETER(active, "Signal this announcement [0 or 1]");

            /* This supports deferred start/stop to allow the user
             * to compensate for audio encoding delay
             */
            RC_ADD_PARAMETER(start_in,
                    "Start signalling this announcement after a delay [ms]");
            RC_ADD_PARAMETER(stop_in,
                    "Stop signalling this announcement after a delay [ms]");
        }

        uint8_t cluster_id = 0;
        uint16_t flags = 0;
        std::string subchanneluid;

        std::string tostring(void) const;

        /* Check if the activation/deactivation timeout occurred,
         * and return of if the Announcement is active
         */
        bool is_active(void);

    private:
        mutable std::mutex m_active_mutex;
        bool m_active = false;
        boost::optional<
            std::chrono::time_point<
                std::chrono::steady_clock> > m_deferred_start_time;
        boost::optional<
            std::chrono::time_point<
                std::chrono::steady_clock> > m_deferred_stop_time;

        /* Remote control */
        virtual void set_parameter(const std::string& parameter,
               const std::string& value);

        /* Getting a parameter always returns a string. */
        virtual const std::string get_parameter(const std::string& parameter) const;
};

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

#define DABLABEL_LENGTH 16

class DabLabel
{
    public:
        /* Set a new label and short label. If the label parses as valid UTF-8, it
         * will be converted to EBU Latin. If utf-8 decoding fails, the label
         * will be used as is. Characters that cannot be converted are replaced
         * by a space.
         *
         * returns:  0 on success
         *          -1 if the short_label is not a representable
         *          -2 if the short_label is too long
         *          -3 if the text is too long
         */
        int setLabel(const std::string& label, const std::string& short_label);

        /* Same as above, but sets the flag to 0xff00, truncating at 8
         * characters.
         *
         * returns:  0 on success
         *          -3 if the text is too long
         */
        int setLabel(const std::string& label);

        /* Write the label to the 16-byte buffer given in buf
         * In the DAB standard, the label is 16 bytes long, and is
         * padded using spaces.
         */
        void writeLabel(uint8_t* buf) const;

        uint16_t flag() const { return m_flag; }
        const std::string long_label() const;
        const std::string short_label() const;

    private:
        /* The flag field selects which label characters make
         * up the short label
         */
        uint16_t m_flag = 0xFFFF;

        /* The m_label is not padded in any way */
        std::string m_label;

        /* Checks and calculates the flag. slabel must be EBU Latin Charset */
        int setShortLabel(const std::string& slabel);
};


class DabService;
class DabComponent;
class DabSubchannel;
class LinkageSet;
struct FrequencyInformation;
struct ServiceOtherEnsembleInfo;

using vec_sp_component = std::vector<std::shared_ptr<DabComponent> >;
using vec_sp_service = std::vector<std::shared_ptr<DabService> >;
using vec_sp_subchannel = std::vector<std::shared_ptr<DabSubchannel> >;


enum class TransmissionMode_e {
    TM_I,
    TM_II,
    TM_III,
    TM_IV
};

class dabEnsemble : public RemoteControllable {
    public:
        dabEnsemble()
            : RemoteControllable("ensemble")
        {
            RC_ADD_PARAMETER(localtimeoffset,
                    "local time offset, 'auto' or -24 to +24 [half-hours]");
        }

        /* Remote control */
        virtual void set_parameter(const std::string& parameter,
               const std::string& value);

        /* Getting a parameter always returns a string. */
        virtual const std::string get_parameter(const std::string& parameter) const;

        /* Check if the Linkage Sets are valid */
        bool validate_linkage_sets(void);

        /* all fields are public, since this was a struct before */
        uint16_t id = 0;
        uint8_t ecc = 0;
        DabLabel label;
        TransmissionMode_e transmission_mode = TransmissionMode_e::TM_I;

        /* Use the local time to calculate the lto */
        bool lto_auto = true;

        int lto = 0; // local time offset in half-hours
        // range: -24 to +24

        // 1 corresponds to the PTy used in RDS
        // 2 corresponds to program types used in north america
        int international_table = 1;

        vec_sp_service services;
        vec_sp_component components;
        vec_sp_subchannel subchannels;

        std::vector<std::shared_ptr<AnnouncementCluster> > clusters;
        std::vector<std::shared_ptr<LinkageSet> > linkagesets;
        std::vector<FrequencyInformation> frequency_information;
        std::vector<ServiceOtherEnsembleInfo> service_other_ensemble;
};


struct dabProtectionUEP {
    unsigned char tableIndex;
};

enum dab_protection_eep_profile {
    EEP_A,
    EEP_B
};

struct dabProtectionEEP {
    dab_protection_eep_profile profile;

    // option is a 3-bit field where 000 and 001 are used to
    // select EEP profile A and B.
    // Other values are for future use, see
    // EN 300 401 Clause 6.2.1 "Basic sub-channel organisation"
    uint8_t GetOption(void) const {
        return (this->profile == EEP_A) ? 0 : 1;
    }
};

enum dab_protection_form_t {
    UEP, // implies FIG0/1 Short form
    EEP  //                Long form
};

struct dabProtection {
    unsigned char level;
    dab_protection_form_t form;
    union {
        dabProtectionUEP uep;
        dabProtectionEEP eep;
    };
};

class DabSubchannel
{
public:
    DabSubchannel(std::string& uid) :
            uid(uid),
            input(),
            id(0),
            type(subchannel_type_t::Audio),
            startAddress(0),
            bitrate(0),
            protection()
    {
    }

    // Calculate subchannel size in number of CU
    unsigned short getSizeCu(void) const;

    // Calculate subchannel size in number of bytes
    unsigned short getSizeByte(void) const;

    // Calculate subchannel size in number of uint32_t
    unsigned short getSizeWord(void) const;

    // Calculate subchannel size in number of uint64_t
    unsigned short getSizeDWord(void) const;

    std::string uid;

    std::string inputUri;
    std::shared_ptr<Inputs::InputBase> input;
    unsigned char id;
    subchannel_type_t type;
    uint16_t startAddress;
    uint16_t bitrate;
    dabProtection protection;
};



struct dabAudioComponent {
    dabAudioComponent() :
        uaType(0xFFFF) {}

    uint16_t uaType; // User Application Type
};


struct dabDataComponent {
};


struct dabFidcComponent {
};


struct dabPacketComponent {
    dabPacketComponent() :
        id(0),
        address(0),
        appType(0xFFFF),
        datagroup(false) { }

    uint16_t id;
    uint16_t address;
    uint16_t appType;
    bool datagroup;
};

class DabComponent : public RemoteControllable
{
    public:
        DabComponent(std::string& uid) :
            RemoteControllable(uid),
            uid(uid)
        {
            RC_ADD_PARAMETER(label, "Label and shortlabel [label,short]");
        }

        std::string uid;

        DabLabel label;
        uint32_t serviceId;
        uint8_t subchId;
        uint8_t type;
        uint8_t SCIdS;

        dabAudioComponent audio;
        dabDataComponent data;
        dabFidcComponent fidc;
        dabPacketComponent packet;

        bool isPacketComponent(vec_sp_subchannel& subchannels) const;

        /* Remote control */
        virtual void set_parameter(const std::string& parameter,
               const std::string& value);

        /* Getting a parameter always returns a string. */
        virtual const std::string get_parameter(const std::string& parameter) const;
};

class DabService : public RemoteControllable
{
    public:
        DabService(std::string& uid) :
            RemoteControllable(uid),
            uid(uid),
            label()
        {
            RC_ADD_PARAMETER(label, "Label and shortlabel [label,short]");
            RC_ADD_PARAMETER(pty, "Programme Type");
            RC_ADD_PARAMETER(ptysd, "PTy Static/Dynamic [static,dynamic]");
        }

        std::string uid;

        uint32_t id = 0;

        /* Services with different ECC than the ensemble must be signalled in FIG0/9.
         * here, leave at 0 if they have the same as the ensemble
         */
        uint8_t ecc = 0;

        struct pty_settings_t {
            uint8_t pty = 0; // 0 means disabled
            bool dynamic_no_static = false;
        };

        pty_settings_t pty_settings;

        unsigned char language = 0;

        /* ASu (Announcement support) flags: this 16-bit flag field shall
         * specify the type(s) of announcements by which it is possible to
         * interrupt the reception of the service. The interpretation of this
         * field shall be as defined in TS 101 756, table 14.
         */
        uint16_t ASu = 0;
        std::vector<uint8_t> clusters;

        subchannel_type_t getType(const std::shared_ptr<dabEnsemble> ensemble) const;
        bool isProgramme(const std::shared_ptr<dabEnsemble>& ensemble) const;
        unsigned char nbComponent(const vec_sp_component& components) const;

        DabLabel label;

        /* Remote control */
        virtual void set_parameter(const std::string& parameter,
               const std::string& value);

        /* Getting a parameter always returns a string. */
        virtual const std::string get_parameter(const std::string& parameter) const;
};

/* Represent an entry for FIG0/24 */
struct ServiceOtherEnsembleInfo {
    uint32_t service_id = 0;
    // Ensembles in which this service is also available
    std::vector<uint16_t> other_ensembles;
};

enum class ServiceLinkType {DAB, FM, DRM, AMSS};

/* Represent one link inside a linkage set */
struct ServiceLink {
    ServiceLinkType type;
    uint16_t id;
    uint8_t ecc;
};

/* Represents a linkage set linkage sets according to
 * TS 103 176 Clause 5.2.3 "Linkage sets". This information will
 * be encoded in FIG 0/6.
 */
class LinkageSet {
    public:
        LinkageSet(const std::string& name,
                uint16_t lsn,
                bool active,
                bool hard,
                bool international);

        std::string get_name(void) const { return m_name; }

        std::list<ServiceLink> id_list;

        /* Linkage Set Number is a 12-bit number that identifies the linkage
         * set in a country (requires coordination between multiplex operators
         * in a country)
         */
        uint16_t lsn;

        bool active; // TODO: Remote-controllable
        bool hard;
        bool international;

        std::string keyservice; // TODO replace by pointer to service

        /* Return a LinkageSet with id_list filtered to include
         * only those links of a given type
         */
        LinkageSet filter_type(const ServiceLinkType type);

    private:
        std::string m_name;
};

// FIG 0/21
enum class RangeModulation {
    dab_ensemble = 0,
    drm = 6,
    fm_with_rds = 8,
    amss = 14
};

struct FrequencyInfoDab {
    enum class ControlField_e {
        adjacent_no_mode = 0,
        adjacent_mode1 = 2,
        disjoint_no_mode = 1,
        disjoint_mode1 = 3};

    struct ListElement {
        std::string uid;
        ControlField_e control_field;
        float frequency;
    };

    uint16_t eid;
    std::vector<ListElement> frequencies;
};

struct FrequencyInfoDrm {
    uint32_t drm_service_id;
    std::vector<float> frequencies;
};

struct FrequencyInfoFm {
    uint16_t pi_code;
    std::vector<float> frequencies;
};

struct FrequencyInfoAmss {
    uint32_t amss_service_id;
    std::vector<float> frequencies;
};

/* One FI database entry. DB key are oe, rm, and idfield, which is
 * inside the fi_XXX field
 */
struct FrequencyInformation {
    std::string uid;
    bool other_ensemble = false;

    // The latest draft spec does not specify the RegionId anymore, it's
    // now a reserved field.
    RangeModulation rm = RangeModulation::dab_ensemble;
    bool continuity = false;

    // Only one of those must contain information, which
    // must be consistent with RangeModulation
    FrequencyInfoDab fi_dab;
    FrequencyInfoDrm fi_drm;
    FrequencyInfoFm fi_fm;
    FrequencyInfoAmss fi_amss;
};

vec_sp_subchannel::iterator getSubchannel(
        vec_sp_subchannel& subchannels,
        int id);

vec_sp_component::iterator getComponent(
        vec_sp_component& components,
        uint32_t serviceId,
        vec_sp_component::iterator current);

vec_sp_component::iterator getComponent(
        vec_sp_component& components,
        uint32_t serviceId);

vec_sp_service::iterator getService(
        std::shared_ptr<DabComponent> component,
        vec_sp_service& services);

