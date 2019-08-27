/*
   Copyright (C) 2019
   Matthias P. Braendli, matthias.braendli@mpb.li

   http://opendigitalradio.org

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#pragma once

#include "common.hpp"
#include <cstdint>
#include <deque>
#include <string>
#include <vector>
#include <array>

namespace EdiDecoder {

// Information for STI-D Management
struct sti_management_data {
    bool stihf;
    bool atstf;
    bool rfadf;
    uint16_t dflc;

    uint32_t tsta;
};

// Information for a subchannel available in EDI
struct sti_payload_data {
    uint16_t stream_index;
    uint8_t rfa;
    uint8_t tid;
    uint8_t tidext;
    bool crcstf;
    uint16_t stid;
    std::vector<uint8_t> istd;

    // Return the length of ISTD in bytes
    uint16_t stl(void) const { return istd.size(); }
};

/* A class that receives STI data must implement the interface described
 * in the STIDataCollector. This can be e.g. a converter to ETI, or something that
 * prepares data structures for a modulator.
 */
class STIDataCollector {
    public:
        // Tell the ETIWriter what EDI protocol we receive in *ptr.
        // This is not part of the ETI data, but is used as check
        virtual void update_protocol(
                const std::string& proto,
                uint16_t major,
                uint16_t minor) = 0;

        // STAT error field and service provider ID
        virtual void update_stat(uint8_t stat, uint16_t spid) = 0;

        // In addition to TSTA in ETI, EDI also transports more time
        // stamp information.
        virtual void update_edi_time(uint32_t utco, uint32_t seconds) = 0;

        virtual void update_rfad(std::array<uint8_t, 9> rfad) = 0;
        virtual void update_sti_management(const sti_management_data& data) = 0;

        virtual void add_payload(sti_payload_data&& payload) = 0;

        virtual void assemble() = 0;
};

/* The STIDecoder takes care of decoding the EDI TAGs related to the transport
 * of ETI(NI) data inside AF and PF packets.
 *
 * PF packets are handed over to the PFT decoder, which will in turn return
 * AF packets. AF packets are directly handled (TAG extraction) here.
 */
class STIDecoder {
    public:
        STIDecoder(STIDataCollector& data_collector, bool verbose);

        /* Push bytes into the decoder. The buf can contain more
         * than a single packet. This is useful when reading from streams
         * (files, TCP)
         */
        void push_bytes(const std::vector<uint8_t> &buf);

        /* Push a complete packet into the decoder. Useful for UDP and other
         * datagram-oriented protocols.
         */
        void push_packet(const std::vector<uint8_t> &buf);

        /* Set the maximum delay in number of AF Packets before we
         * abandon decoding a given pseq.
         */
        void setMaxDelay(int num_af_packets);

    private:
        bool decode_starptr(const std::vector<uint8_t> &value, uint16_t);
        bool decode_dsti(const std::vector<uint8_t> &value, uint16_t);
        bool decode_ssn(const std::vector<uint8_t> &value, uint16_t n);
        bool decode_stardmy(const std::vector<uint8_t> &value, uint16_t);

        void packet_completed();

        STIDataCollector& m_data_collector;
        TagDispatcher m_dispatcher;

};

}
