/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2017
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

#pragma once

#include <cstdint>
#include <map>
#include <set>

#include "fig/FIG.h"

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


/* Warning: When bit SCCA_flag is unset(0), the multiplexer R&S does not send
 * the SCCA field. But, in the Factum ETI analyzer, if this field is not there,
 * it is an error.
 */
struct FIGtype0_3 {
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

struct FIGtype0_5_short {
    uint8_t SubChId:6;
    uint8_t rfu:1;
    uint8_t LS:1;

    uint8_t language;
} PACKED;

struct FIGtype0_6 {
    uint8_t LSN_high:4; // Linkage Set Number
    uint8_t ILS:1; // 0=national / 1=international
    uint8_t SH:1; // 0=Soft link / 1=Hard link
    uint8_t LA:1; // Linkage actuator
    uint8_t IdListFlag:1; // Marks the presence of the list

    uint8_t LSN_low; // Linkage Set Number

    void setLSN(uint16_t LSN) {
        LSN_high = LSN >> 8;
        LSN_low = LSN & 0xff;
    }
} PACKED;

#define FIG0_6_IDLQ_DAB 0x0
#define FIG0_6_IDLQ_RDS 0x1
#define FIG0_6_IDLQ_DRM_AMSS 0x3

struct FIGtype0_6_header {
    uint8_t num_ids:4; // number of Ids to follow in the list
    uint8_t rfa:1; // must be 0
    uint8_t IdLQ:2; // Identifier List Qualifier, see above defines
    uint8_t rfu:1; // must be 0
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
    uint8_t rfa1:1;
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


struct FIGtype0_17 {
    uint16_t SId;

    uint8_t rfa2_high:4;
    uint8_t rfu1:2;
    uint8_t rfa1:1;
    uint8_t SD:1; // Static/Dynamic

    uint8_t IntCode:5;
    uint8_t rfu2:1;
    uint8_t rfa2_low:2;
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


struct FIGtype0_21_header {
    uint16_t rfa:11;
    uint16_t length_fi:5;
} PACKED;

struct FIGtype0_21_fi_list_header {
    uint16_t id;
    uint8_t range_modulation:4;
    uint8_t continuity:1;
    uint8_t length_freq_list:3;
} PACKED;

struct FIGtype0_21_fi_dab_entry {
    uint8_t control_field:5;
    uint8_t freqHigh:3;
    uint16_t freqLow;

    void setFreq(uint32_t freq) {
        freqHigh = (freq >> 16) & 0x7;
        freqLow = freq & 0xffff;
    }
} PACKED;

#ifdef _WIN32
#   pragma pack(pop)
#endif

