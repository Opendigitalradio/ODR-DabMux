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

#ifndef __DAB_MULTIPLEXER_H__
#define __DAB_MULTIPLEXER_H__

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include "dabOutput/dabOutput.h"
#include "dabOutput/edi/TagItems.h"
#include "dabOutput/edi/TagPacket.h"
#include "dabOutput/edi/AFPacket.h"
#include "dabOutput/edi/PFT.h"
#include "fig/FIGCarousel.h"
#include "crc.h"
#include "utils.h"
#include "UdpSocket.h"
#include "InetAddress.h"
#include "PcDebug.h"
#include "MuxElements.h"
#include "RemoteControl.h"
#include "Eti.h"
#include "ClockTAI.h"
#include <vector>
#include <chrono>
#include <memory>
#include <string>
#include <memory>
#include <boost/property_tree/ptree.hpp>

class DabMultiplexer : public RemoteControllable {
    public:
        DabMultiplexer(
                std::shared_ptr<BaseRemoteController> rc,
                boost::property_tree::ptree pt);
        void prepare(void);

        unsigned long getCurrentFrame() { return currentFrame; }

        void mux_frame(std::vector<std::shared_ptr<DabOutput> >& outputs);

        void print_info(void);

        void set_edi_config(const edi_configuration_t& new_edi_conf);

        /* Remote control */
        virtual void set_parameter(const std::string& parameter,
               const std::string& value);

        /* Getting a parameter always returns a string. */
        virtual const std::string get_parameter(const std::string& parameter) const;

    private:
        void prepare_subchannels(void);
        void prepare_services_components(void);
        void prepare_data_inputs(void);

        boost::property_tree::ptree m_pt;
        std::shared_ptr<BaseRemoteController> m_rc;

        unsigned timestamp;
        bool MNSC_increment_time;
        struct timeval mnsc_time;
        std::chrono::system_clock::time_point edi_time;

        edi_configuration_t edi_conf;

        uint32_t sync;
        unsigned long currentFrame;

        std::shared_ptr<dabEnsemble> ensemble;

        ClockTAI m_clock_tai;

#if HAVE_OUTPUT_EDI
        std::ofstream edi_debug_file;

        // The TagPacket will then be placed into an AFPacket
        AFPacketiser edi_afPacketiser;

        // The AF Packet will be protected with reed-solomon and split in fragments
        PFT edi_pft;
#endif // HAVE_OUTPUT_EDI

        /* New FIG Carousel */
        FIC::FIGCarousel fig_carousel;
};

// DAB Mode
#define DEFAULT_DAB_MODE    1

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

#endif

