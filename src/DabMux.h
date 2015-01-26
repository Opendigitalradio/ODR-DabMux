/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2014
   Matthias P. Braendli, matthias.braendli@mpb.li

   This file declares several structures used in the multiplexer,
   and defines default values for some parameters.
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
#ifndef _DABMUX_H
#define _DABMUX_H

#include <stdint.h>
#include <string>
#include <vector>
#include "RemoteControl.h"
#include "dabOutput/dabOutput.h"
#include "dabInput.h"
#include "Eti.h"
#include "MuxElements.h"

#ifdef _WIN32
#   include <time.h>
#else
#   include <sys/time.h>
#endif


// DAB Mode
#define DEFAULT_DAB_MODE    2

// Taille de la trame de donnee, sous-canal 3, nb de paquets de 64bits,
// STL3 * 8 = x kbytes par trame ETI

// Data bitrate in kbits/s. Must be 64 kb/s multiple.
#define DEFAULT_DATA_BITRATE    384
#define DEFAULT_PACKET_BITRATE  32

/* default ensemble parameters. Label must be max 16 chars, short label
 * a subset of the label, max 8 chars
 */
#define DEFAULT_ENSEMBLE_LABEL          "ODR Dab Mux"
#define DEFAULT_ENSEMBLE_SHORT_LABEL    "ODRMux"
#define DEFAULT_ENSEMBLE_ID             0xc000
#define DEFAULT_ENSEMBLE_ECC            0xa1

// start value for default service IDs (if not overridden by configuration)
#define DEFAULT_SERVICE_ID      50
#define DEFAULT_PACKET_ADDRESS  0

/*****************************************************************************
 *****************   Definition of FIG structures ****************************
 *****************************************************************************/
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
    uint32_t xpad;
} PACKED;

#define FIG0_13_APPTYPE_SLIDESHOW  0x2
#define FIG0_13_APPTYPE_WEBSITE    0x3
#define FIG0_13_APPTYPE_TPEG       0x4
#define FIG0_13_APPTYPE_DGPS       0x5
#define FIG0_13_APPTYPE_TMC        0x6
#define FIG0_13_APPTYPE_EPG        0x7
#define FIG0_13_APPTYPE_DABJAVA    0x8
#define FIG0_13_APPTYPE_JOURNALINE 0x441


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

#endif

