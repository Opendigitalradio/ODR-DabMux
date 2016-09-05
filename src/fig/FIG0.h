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

#ifndef __FIG0_H_
#define __FIG0_H_

#include <cstdint>
#include <map>
#include <set>

#include "fig/FIG.h"

namespace FIC {

// FIG type 0/0, Multiplex Configuration Info (MCI),
// Ensemble information
class FIG0_0 : public IFIG
{
    public:
        FIG0_0(FIGRuntimeInformation* rti) :
            m_rti(rti) {}
        virtual FillStatus fill(uint8_t *buf, size_t max_size);
        virtual FIG_rate repetition_rate(void) { return FIG_rate::FIG0_0; }

        virtual const int figtype(void) const { return 0; }
        virtual const int figextension(void) const { return 0; }

    private:
        FIGRuntimeInformation *m_rti;
};

// FIG type 0/1, MIC, Sub-Channel Organization,
// one instance of the part for each subchannel
class FIG0_1 : public IFIG
{
    public:
        FIG0_1(FIGRuntimeInformation* rti);
        virtual FillStatus fill(uint8_t *buf, size_t max_size);
        virtual FIG_rate repetition_rate(void) { return FIG_rate::A; }

        virtual const int figtype(void) const { return 0; }
        virtual const int figextension(void) const { return 1; }

    private:
        FIGRuntimeInformation *m_rti;
        bool m_initialised;
        std::vector<DabSubchannel*>           subchannels;
        std::vector<DabSubchannel*>::iterator subchannelFIG0_1;

        uint8_t m_watermarkData[128];
        size_t  m_watermarkSize;
        size_t  m_watermarkPos;
};

// FIG type 0/2, MCI, Service Organization, one instance of
// FIGtype0_2_Service for each subchannel
class FIG0_2 : public IFIG
{
    public:
        FIG0_2(FIGRuntimeInformation* rti);
        virtual FillStatus fill(uint8_t *buf, size_t max_size);
        virtual FIG_rate repetition_rate(void) { return FIG_rate::A; }

        virtual const int figtype(void) const { return 0; }
        virtual const int figextension(void) const { return 2; }

    private:
        FIGRuntimeInformation *m_rti;
        bool m_initialised;
        bool m_inserting_audio_not_data;
        std::vector<std::shared_ptr<DabService> > m_audio_services;
        std::vector<std::shared_ptr<DabService> > m_data_services;
        std::vector<std::shared_ptr<DabService> >::iterator serviceFIG0_2;
};

// FIG type 0/3
// The Extension 3 of FIG type 0 (FIG 0/3) gives additional information about
// the service component description in packet mode.
class FIG0_3 : public IFIG
{
    public:
        FIG0_3(FIGRuntimeInformation* rti);
        virtual FillStatus fill(uint8_t *buf, size_t max_size);
        virtual FIG_rate repetition_rate(void) { return FIG_rate::A; }

        virtual const int figtype(void) const { return 0; }
        virtual const int figextension(void) const { return 3; }

    private:
        FIGRuntimeInformation *m_rti;
        bool m_initialised;
        std::vector<DabComponent*>::iterator componentFIG0_3;
};

// FIG type 0/8
// The Extension 8 of FIG type 0 (FIG 0/8) provides information to link
// together the service component description that is valid within the ensemble
// to a service component description that is valid in other ensembles
class FIG0_8 : public IFIG
{
    public:
        FIG0_8(FIGRuntimeInformation* rti);
        virtual FillStatus fill(uint8_t *buf, size_t max_size);
        virtual FIG_rate repetition_rate(void) { return FIG_rate::B; }

        virtual const int figtype(void) const { return 0; }
        virtual const int figextension(void) const { return 8; }

    private:
        FIGRuntimeInformation *m_rti;
        bool m_initialised;
        bool m_transmit_programme;
        std::vector<DabComponent*>::iterator componentFIG0_8;
};

// FIG type 0/9
// The Country, LTO and International table feature defines the local time
// offset, the International Table and the Extended Country Code (ECC)
class FIG0_9 : public IFIG
{
    public:
        FIG0_9(FIGRuntimeInformation* rti);
        virtual FillStatus fill(uint8_t *buf, size_t max_size);
        virtual FIG_rate repetition_rate(void) { return FIG_rate::B; }

        virtual const int figtype(void) const { return 0; }
        virtual const int figextension(void) const { return 9; }

    private:
        FIGRuntimeInformation *m_rti;
};


// FIG type 0/10
// The date and time feature is used to signal a location-independent timing
// reference in UTC format.
class FIG0_10 : public IFIG
{
    public:
        FIG0_10(FIGRuntimeInformation* rti);
        virtual FillStatus fill(uint8_t *buf, size_t max_size);
        virtual FIG_rate repetition_rate(void) { return FIG_rate::B; }

        virtual const int figtype(void) const { return 0; }
        virtual const int figextension(void) const { return 10; }

    private:
        FIGRuntimeInformation *m_rti;
};

// FIG type 0/13
// User Application Information
class FIG0_13 : public IFIG
{
    public:
        FIG0_13(FIGRuntimeInformation* rti);
        virtual FillStatus fill(uint8_t *buf, size_t max_size);
        virtual FIG_rate repetition_rate(void) { return FIG_rate::B; }

        virtual const int figtype(void) const { return 0; }
        virtual const int figextension(void) const { return 13; }

    private:
        FIGRuntimeInformation *m_rti;
        bool m_initialised;
        bool m_transmit_programme;
        std::vector<DabComponent*>::iterator componentFIG0_13;
};

// FIG type 0/17
// Programme Type (PTy)
class FIG0_17 : public IFIG
{
    public:
        FIG0_17(FIGRuntimeInformation* rti);
        virtual FillStatus fill(uint8_t *buf, size_t max_size);
        virtual FIG_rate repetition_rate(void) { return FIG_rate::A_B; }

        virtual const int figtype(void) const { return 0; }
        virtual const int figextension(void) const { return 17; }

    private:
        FIGRuntimeInformation *m_rti;
        bool m_initialised;
        std::vector<std::shared_ptr<DabService> >::iterator serviceFIG0_17;
};

// FIG type 0/18
class FIG0_18 : public IFIG
{
    public:
        FIG0_18(FIGRuntimeInformation* rti);
        virtual FillStatus fill(uint8_t *buf, size_t max_size);
        virtual FIG_rate repetition_rate(void) { return FIG_rate::B; }

        virtual const int figtype(void) const { return 0; }
        virtual const int figextension(void) const { return 18; }

    private:
        FIGRuntimeInformation *m_rti;
        bool m_initialised;
        std::vector<std::shared_ptr<DabService> >::iterator service;
};

// FIG type 0/19
class FIG0_19 : public IFIG
{
    public:
        FIG0_19(FIGRuntimeInformation* rti);
        virtual FillStatus fill(uint8_t *buf, size_t max_size);
        virtual FIG_rate repetition_rate(void);

        virtual const int figtype(void) const { return 0; }
        virtual const int figextension(void) const { return 19; }

    private:
        FIGRuntimeInformation *m_rti;

        void update_state(void);

        /* When a new announcement gets active, it is moved into the list
         * of new announcements, and gets transmitted at a faster rate for
         * two seconds.
         * Same for recently disabled announcements.
         */

        /* Map of cluster to frame count */
        std::map<
            std::shared_ptr<AnnouncementCluster>,int> m_new_announcements;

        std::set<
            std::shared_ptr<AnnouncementCluster> > m_repeated_announcements;

        /* Map of cluster to frame count */
        std::map<
            std::shared_ptr<AnnouncementCluster>,int> m_disabled_announcements;
};

#ifdef _WIN32
#   pragma pack(push)
#endif

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


struct FIGtype0_10_LongForm {
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

    uint8_t Milliseconds_high:2;
    uint8_t Seconds:6;

    uint8_t Milliseconds_low;

    void setMJD(uint32_t date) {
        MJD_high = (date >> 10) & 0x7f;
        MJD_med = (date >> 2) & 0xff;
        MJD_low = date & 0x03;
    }
    void setHours(uint16_t hours) {
        Hours_high = (hours >> 2) & 0x07;
        Hours_low = hours & 0x03;
    }

    void setMilliseconds(uint16_t ms) {
        Milliseconds_high = (ms >> 8) & 0x03;
        Milliseconds_low = ms & 0xFF;
    }
} PACKED;


struct FIGtype0_17_programme {
    uint16_t SId;
    uint8_t NFC:2;
    uint8_t Rfa:2;
    uint8_t CC:1; // Complimentary code
    uint8_t L:1; // Signals presence of language field
    uint8_t PS:1; // Primary/Secondary
    // PS==0: language refers to primary service component
    // PS==1: language refers to secondary service component
    uint8_t SD:1; // Static/Dynamic
    // SD==0: PTy and language may not represent the current programme contents
    // SD==1: PTy and language represent the current programme contents
} PACKED;

struct FIGtype0_18 {
    uint16_t SId;
    uint16_t ASu;
    uint8_t  NumClusters:5;
    uint8_t  Rfa:3;
    /* Followed by uint8_t Cluster IDs */
} PACKED;

struct FIGtype0_19 {
    uint8_t  ClusterId;
    uint16_t ASw;
    uint8_t  SubChId:6;
    uint8_t  RegionFlag:1; // shall be zero
    uint8_t  NewFlag:1;
    // Region and RFa not supported
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


// See EN 300 401, Clause 8.1.20 for the FIG0_13 description
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
    uint16_t xpad;
} PACKED;

#define FIG0_13_APPTYPE_SLIDESHOW  0x2
#define FIG0_13_APPTYPE_WEBSITE    0x3
#define FIG0_13_APPTYPE_TPEG       0x4
#define FIG0_13_APPTYPE_DGPS       0x5
#define FIG0_13_APPTYPE_TMC        0x6
#define FIG0_13_APPTYPE_EPG        0x7
#define FIG0_13_APPTYPE_DABJAVA    0x8
#define FIG0_13_APPTYPE_JOURNALINE 0x441


#ifdef _WIN32
#   pragma pack(pop)
#endif

} // namespace FIC

#endif // __FIG0_H_

