/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2015
   Matthias P. Braendli, matthias.braendli@mpb.li

   Implementation of FIG1
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

#include "fig/FIG1.h"
#include "DabMultiplexer.h"

namespace FIC {

//=========== FIG 1/0 ===========

FillStatus FIG1_0::fill(uint8_t *buf, size_t max_size)
{
    FillStatus fs;
    auto ensemble = m_rti->ensemble;
    size_t remaining = max_size;

    if (remaining < 22) {
        fs.num_bytes_written = 0;
        return fs;
    }

    auto fig1_0 = (FIGtype1_0*)buf;

    fig1_0->Length = 21;
    fig1_0->FIGtypeNumber = 1;
    fig1_0->Extension = 0;
    fig1_0->OE = 0;
    fig1_0->Charset = 0;
    fig1_0->EId = htons(ensemble->id);
    buf += 4;

    ensemble->label.writeLabel(buf);
    buf += 16;

    buf[0] = ensemble->label.flag() >> 8;
    buf[1] = ensemble->label.flag() & 0xFF;
    buf += 2;

    remaining -= 22;

    fs.complete_fig_transmitted = true;
    fs.num_bytes_written = max_size - remaining;
    return fs;
}

//=========== FIG 1/1 ===========

FillStatus FIG1_1::fill(uint8_t *buf, size_t max_size)
{
    FillStatus fs;

    ssize_t remaining = max_size;

    if (not m_initialised) {
        service = m_rti->ensemble->services.end();
        m_initialised = true;
    }

    auto ensemble = m_rti->ensemble;

    // Rotate through the subchannels until there is no more
    // space
    if (service == ensemble->services.end()) {
        service = ensemble->services.begin();
        fs.complete_fig_transmitted = true;
    }

    for (; service != ensemble->services.end();
            ++service) {

        if (remaining < 4 + 16 + 2) {
            break;
        }

        if ((*service)->getType(ensemble) == subchannel_type_t::Audio) {
            auto fig1_1 = (FIGtype1_1 *)buf;

            fig1_1->FIGtypeNumber = 1;
            fig1_1->Length = 21;
            fig1_1->Charset = 0;
            fig1_1->OE = 0;
            fig1_1->Extension = 1;

            fig1_1->Sld = htons((*service)->id);
            buf += 4;
            remaining -= 4;

            (*service)->label.writeLabel(buf);
            buf += 16;
            remaining -= 16;

            buf[0] = (*service)->label.flag() >> 8;
            buf[1] = (*service)->label.flag() & 0xFF;
            buf += 2;
            remaining -= 2;
        }
    }

    fs.num_bytes_written = max_size - remaining;
    return fs;
}

//=========== FIG 1/4 ===========

FillStatus FIG1_4::fill(uint8_t *buf, size_t max_size)
{
    FillStatus fs;

    ssize_t remaining = max_size;

    if (not m_initialised) {
        component = m_rti->ensemble->components.end();
        m_initialised = true;
    }

    auto ensemble = m_rti->ensemble;

    // Rotate through the subchannels until there is no more
    // space
    if (component == ensemble->components.end()) {
        component = ensemble->components.begin();
        fs.complete_fig_transmitted = true;
    }

    for (; component != ensemble->components.end();
            ++component) {

        auto service = getService(*component, ensemble->services);

        if (not (*component)->label.long_label().empty() ) {
            if ((*service)->getType(ensemble) == subchannel_type_t::Audio) {

                if (remaining < 5 + 16 + 2) {
                    break;
                }

                // Programme
                FIGtype1_4_programme *fig1_4;
                fig1_4 = (FIGtype1_4_programme*)buf;

                fig1_4->FIGtypeNumber = 1;
                fig1_4->Length = 22;
                fig1_4->Charset = 0;
                fig1_4->OE = 0;
                fig1_4->Extension = 4;
                fig1_4->PD = 0;
                fig1_4->rfa = 0;
                fig1_4->SCIdS = (*component)->SCIdS;

                fig1_4->SId = htons((*service)->id);
                buf += 5;
                remaining -= 5;
            }
            else {    // Data

                if (remaining < 7 + 16 + 2) {
                    break;
                }

                FIGtype1_4_data *fig1_4;
                fig1_4 = (FIGtype1_4_data *)buf;
                fig1_4->FIGtypeNumber = 1;
                fig1_4->Length = 24;
                fig1_4->Charset = 0;
                fig1_4->OE = 0;
                fig1_4->Extension = 4;
                fig1_4->PD = 1;
                fig1_4->rfa = 0;
                fig1_4->SCIdS = (*component)->SCIdS;

                fig1_4->SId = htonl((*service)->id);
                buf += 7;
                remaining -= 7;
            }
            (*component)->label.writeLabel(buf);
            buf += 16;
            remaining -= 16;

            buf[0] = (*component)->label.flag() >> 8;
            buf[1] = (*component)->label.flag() & 0xFF;
            buf += 2;
            remaining -= 2;
        }
    }

    fs.num_bytes_written = max_size - remaining;
    return fs;
}

//=========== FIG 1/5 ===========

FillStatus FIG1_5::fill(uint8_t *buf, size_t max_size)
{
    FillStatus fs;

    ssize_t remaining = max_size;

    if (not m_initialised) {
        service = m_rti->ensemble->services.end();
        m_initialised = true;
    }

    auto ensemble = m_rti->ensemble;

    // Rotate through the subchannels until there is no more
    // space
    if (service == ensemble->services.end()) {
        service = ensemble->services.begin();
        fs.complete_fig_transmitted = true;
    }

    for (; service != ensemble->services.end();
            ++service) {

        if (remaining < 6 + 16 + 2) {
            break;
        }

        if ((*service)->getType(ensemble) != subchannel_type_t::Audio) {
            auto fig1_5 = (FIGtype1_5 *)buf;
            fig1_5->FIGtypeNumber = 1;
            fig1_5->Length = 23;
            fig1_5->Charset = 0;
            fig1_5->OE = 0;
            fig1_5->Extension = 5;

            fig1_5->SId = htonl((*service)->id);
            buf += 6;
            remaining -= 6;

            (*service)->label.writeLabel(buf);
            buf += 16;
            remaining -= 16;

            buf[0] = (*service)->label.flag() >> 8;
            buf[1] = (*service)->label.flag() & 0xFF;
            buf += 2;
            remaining -= 2;
        }
    }

    fs.num_bytes_written = max_size - remaining;
    return fs;
}

} // namespace FIC

