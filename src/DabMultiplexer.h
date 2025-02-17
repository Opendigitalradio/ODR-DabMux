/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2025
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

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include "dabOutput/dabOutput.h"
#include "edioutput/Transport.h"
#include "fig/FIGCarousel.h"
#include "MuxElements.h"
#include "RemoteControl.h"
#include "ClockTAI.h"
#include <vector>
#include <memory>
#include <string>
#include <memory>
#include <boost/property_tree/ptree.hpp>

constexpr uint32_t ETI_FSYNC1 = 0x49C5F8;

class MuxTime {
    private:
    uint32_t m_timestamp = 0;
    std::time_t m_edi_time = 0;

    public:
    std::pair<uint32_t, std::time_t> get_time();

    double tist_offset = 0;

    /* Pre v3 odr-dabmux did the MNSC calculation differently,
     * which works with the easydabv2. The rework in odr-dabmux,
     * deriving MNSC time from EDI time broke this.
     *
     * That's why we're now tracking MNSC time in separate variables,
     * to get the same behaviour back.
     *
     * I'm not aware of any devices using MNSC time besides the
     * easydab. ODR-DabMod now considers EDI seconds or ZMQ metadata.
     */
    bool mnsc_increment_time = false;
    std::time_t mnsc_time = 0;

    /* Setup the time and return the initial currentFrame counter value */
    uint64_t init(uint32_t tist_at_fct0_us);
    void increment_timestamp();
};

class DabMultiplexer : public RemoteControllable {
    public:
        DabMultiplexer(boost::property_tree::ptree pt);
        DabMultiplexer(const DabMultiplexer& other) = delete;
        DabMultiplexer& operator=(const DabMultiplexer& other) = delete;
        ~DabMultiplexer();

        void prepare(bool require_tai_clock);

        uint64_t getCurrentFrame() const { return currentFrame; }

        void mux_frame(std::vector<std::shared_ptr<DabOutput> >& outputs);

        void print_info(void);

        void set_edi_config(const edi::configuration_t& new_edi_conf);

        /* Remote control */
        virtual void set_parameter(const std::string& parameter,
               const std::string& value);

        /* Getting a parameter always returns a string. */
        virtual const std::string get_parameter(const std::string& parameter) const;

        virtual const json::map_t get_all_values() const;

    private:
        void prepare_subchannels(void);
        void prepare_services_components(void);
        void prepare_data_inputs(void);

        boost::property_tree::ptree m_pt;

        MuxTime m_time;
        uint64_t currentFrame = 0;

        edi::configuration_t edi_conf;
        std::shared_ptr<edi::Sender> edi_sender;

        std::shared_ptr<dabEnsemble> ensemble;

        bool m_tai_clock_required = false;
        ClockTAI m_clock_tai;

        /* New FIG Carousel */
        FIC::FIGCarousel fig_carousel;
};
