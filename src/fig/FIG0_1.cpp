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

#include "fig/FIG0_1.h"
#include "utils.h"

namespace FIC {

FIG0_1::FIG0_1(FIGRuntimeInformation *rti) :
    m_rti(rti),
    m_initialised(false),
    m_watermarkSize(0),
    m_watermarkPos(0)
{
    uint8_t buffer[sizeof(m_watermarkData) / 2];
    snprintf((char*)buffer, sizeof(buffer),
            "%s %s, %s %s",
            PACKAGE_NAME,
#if defined(GITVERSION)
            GITVERSION,
#else
            PACKAGE_VERSION,
#endif
            __DATE__, __TIME__);

    memset(m_watermarkData, 0, sizeof(m_watermarkData));
    m_watermarkData[0] = 0x55; // Sync
    m_watermarkData[1] = 0x55;
    m_watermarkSize = 16;
    for (unsigned i = 0; i < strlen((char*)buffer); ++i) {
        for (int j = 0; j < 8; ++j) {
            uint8_t bit = (buffer[m_watermarkPos >> 3] >> (7 - (m_watermarkPos & 0x07))) & 1;
            m_watermarkData[m_watermarkSize >> 3] |= bit << (7 - (m_watermarkSize & 0x07));
            ++m_watermarkSize;
            bit = 1;
            m_watermarkData[m_watermarkSize >> 3] |= bit << (7 - (m_watermarkSize & 0x07));
            ++m_watermarkSize;
            ++m_watermarkPos;
        }
    }
    m_watermarkPos = 0;
}

FillStatus FIG0_1::fill(uint8_t *buf, size_t max_size)
{
#define FIG0_1_TRACE discard

    FillStatus fs;
    size_t remaining = max_size;

    etiLog.level(FIG0_1_TRACE) << "FIG0_1::fill initialised=" <<
        (m_initialised ? 1 : 0);

    const int watermark_bit = (m_watermarkData[m_watermarkPos >> 3] >>
            (7 - (m_watermarkPos & 0x07))) & 1;

    const bool iterate_forward = (watermark_bit == 1);

    if (not m_initialised) {
        m_initialised = true;

        subchannels = m_rti->ensemble->subchannels;

        if (not iterate_forward) {
            std::reverse(subchannels.begin(), subchannels.end());
        }
        subchannelFIG0_1 = subchannels.begin();
    }

    if (max_size < 6) {
        return fs;
    }

    FIGtype0_1 *figtype0_1 = NULL;

    // Rotate through the subchannels until there is no more
    // space in the FIG0/1
    for (; subchannelFIG0_1 != subchannels.end(); ++subchannelFIG0_1 ) {
        size_t subch_iter_ix = std::distance(subchannels.begin(), subchannelFIG0_1);

        etiLog.level(FIG0_1_TRACE) << "FIG0_1::fill loop ix=" << subch_iter_ix;

        dabProtection* protection = &(*subchannelFIG0_1)->protection;

        if (figtype0_1 == NULL) {
            etiLog.level(FIG0_1_TRACE) << "FIG0_1::fill header " <<
                 (protection->form == UEP ? "UEP " : "EEP ") << remaining;

            if ( (protection->form == UEP && remaining < 2 + 3) ||
                 (protection->form == EEP && remaining < 2 + 4) ) {
                etiLog.level(FIG0_1_TRACE) << "FIG0_1::fill no space for header";
                break;
            }

            figtype0_1 = (FIGtype0_1*)buf;

            figtype0_1->FIGtypeNumber = 0;
            figtype0_1->Length = 1;
            figtype0_1->CN = 0;
            figtype0_1->OE = 0;
            figtype0_1->PD = 0;
            figtype0_1->Extension = 1;
            buf += 2;
            remaining -= 2;
        }
        else if ( (protection->form == UEP && remaining < 3) ||
             (protection->form == EEP && remaining < 4) ) {
            etiLog.level(FIG0_1_TRACE) << "FIG0_1::fill no space for fig " <<
                 (protection->form == UEP ? "UEP " : "EEP ") << remaining;
            break;
        }

        if (protection->form == UEP) {
            FIG_01_SubChannel_ShortF *fig0_1subchShort =
                (FIG_01_SubChannel_ShortF*)buf;
            fig0_1subchShort->SubChId = (*subchannelFIG0_1)->id;

            fig0_1subchShort->StartAdress_high =
                (*subchannelFIG0_1)->startAddress / 256;
            fig0_1subchShort->StartAdress_low =
                (*subchannelFIG0_1)->startAddress % 256;

            fig0_1subchShort->Short_Long_form = 0;
            fig0_1subchShort->TableSwitch = 0;
            fig0_1subchShort->TableIndex =
                protection->uep.tableIndex;

            buf += 3;
            remaining -= 3;
            figtype0_1->Length += 3;

            etiLog.level(FIG0_1_TRACE) << "FIG0_1::fill insert UEP id=" <<
               (int)fig0_1subchShort->SubChId << " rem=" << remaining
               << " ix=" << subch_iter_ix;
        }
        else if (protection->form == EEP) {
            FIG_01_SubChannel_LongF *fig0_1subchLong1 =
                (FIG_01_SubChannel_LongF*)buf;
            fig0_1subchLong1->SubChId = (*subchannelFIG0_1)->id;

            fig0_1subchLong1->StartAdress_high =
                (*subchannelFIG0_1)->startAddress / 256;
            fig0_1subchLong1->StartAdress_low =
                (*subchannelFIG0_1)->startAddress % 256;

            fig0_1subchLong1->Short_Long_form = 1;
            fig0_1subchLong1->Option = protection->eep.GetOption();
            fig0_1subchLong1->ProtectionLevel =
                protection->level;

            fig0_1subchLong1->Sub_ChannelSize_high =
                (*subchannelFIG0_1)->getSizeCu() / 256;
            fig0_1subchLong1->Sub_ChannelSize_low =
                (*subchannelFIG0_1)->getSizeCu() % 256;

            buf += 4;
            remaining -= 4;
            figtype0_1->Length += 4;

            etiLog.level(FIG0_1_TRACE) << "FIG0_1::fill insert EEP id=" <<
               (int)fig0_1subchLong1->SubChId << " rem=" << remaining
               << " ix=" << subch_iter_ix;
        }
    }

    size_t subch_iter_ix = std::distance(subchannels.begin(), subchannelFIG0_1);

    etiLog.level(FIG0_1_TRACE) << "FIG0_1::fill loop out, rem=" << remaining
        << " ix=" << subch_iter_ix;

    if (subchannelFIG0_1 == subchannels.end()) {
        etiLog.level(FIG0_1_TRACE) << "FIG0_1::fill completed, rem=" << remaining;
        m_initialised = false;
        fs.complete_fig_transmitted = true;

        m_watermarkPos++;
        if (m_watermarkPos == m_watermarkSize) {
            m_watermarkPos = 0;
        }
    }

    fs.num_bytes_written = max_size - remaining;
    etiLog.level(FIG0_1_TRACE) << "FIG0_1::fill wrote " << fs.num_bytes_written;
    return fs;
}

}
