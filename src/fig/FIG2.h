/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2019
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

#ifndef __FIG2_H_
#define __FIG2_H_

#include <cstdint>
#include <map>

#include "fig/FIG.h"

namespace FIC {

class FIG2_Segments {
    public:
        void clear();
        void load(const std::string& label);

        size_t segment_count() const;
        std::vector<uint8_t> advance_segment();
        size_t current_segment_length() const;
        size_t current_segment_index() const;

        bool ready() const;
        bool complete() const;
        int toggle_flag() const;

    private:
        using vv = std::vector<std::vector<uint8_t> >;
        vv segments;
        vv::iterator current_segment_it;

        std::string label_on_last_load;
        bool toggle = true;
};

// FIG type 2/0, Multiplex Configuration Info (MCI),
// Ensemble information
class FIG2_0 : public IFIG
{
    public:
        FIG2_0(FIGRuntimeInformation* rti) :
            m_rti(rti) {}
        virtual FillStatus fill(uint8_t *buf, size_t max_size);
        virtual FIG_rate repetition_rate() const { return FIG_rate::B; }

        virtual int figtype() const { return 2; }
        virtual int figextension() const { return 0; }

    private:
        FIGRuntimeInformation *m_rti;
        FIG2_Segments m_segments;
};

// FIG type 2/1, programme service label
class FIG2_1 : public IFIG
{
    public:
        FIG2_1(FIGRuntimeInformation* rti) :
            m_rti(rti), m_initialised(false) {}
        virtual FillStatus fill(uint8_t *buf, size_t max_size);
        virtual FIG_rate repetition_rate() const { return FIG_rate::B; }

        virtual int figtype() const { return 2; }
        virtual int figextension() const { return 1; }

    private:
        FIGRuntimeInformation *m_rti;
        bool m_initialised;
        vec_sp_service::iterator service;
        std::map<uint32_t, FIG2_Segments> segment_per_service;
};

// FIG type 2/4, service component label
class FIG2_4 : public IFIG
{
    public:
        FIG2_4(FIGRuntimeInformation* rti) :
            m_rti(rti), m_initialised(false) {}
        virtual FillStatus fill(uint8_t *buf, size_t max_size);
        virtual FIG_rate repetition_rate() const { return FIG_rate::B; }

        virtual int figtype() const { return 2; }
        virtual int figextension() const { return 4; }

    private:
        FIGRuntimeInformation *m_rti;
        bool m_initialised;
        vec_sp_component::iterator component;
        std::map<std::pair<uint32_t, uint8_t>, FIG2_Segments> segment_per_component;
};

// FIG type 2/5, data service label
class FIG2_5 : public IFIG
{
    public:
        FIG2_5(FIGRuntimeInformation* rti) :
            m_rti(rti), m_initialised(false) {}
        virtual FillStatus fill(uint8_t *buf, size_t max_size);
        virtual FIG_rate repetition_rate() const { return FIG_rate::B; }

        virtual int figtype() const { return 2; }
        virtual int figextension() const { return 5; }

    private:
        FIGRuntimeInformation *m_rti;
        bool m_initialised;
        vec_sp_service::iterator service;
};

#ifdef _WIN32
#   pragma pack(push)
#endif

struct FIGtype2 {
    uint8_t Length:5;
    uint8_t FIGtypeNumber:3;

    uint8_t Extension:3;
    uint8_t Rfu:1;
    uint8_t SegmentIndex:3;
    uint8_t ToggleFlag:1;
} PACKED;

struct FIGtype2_4_Programme_Identifier {
    uint8_t SCIdS:4;
    uint8_t rfa:3;
    uint8_t PD:1;
    uint16_t SId;
} PACKED;

struct FIGtype2_4_Data_Identifier {
    uint8_t SCIdS:4;
    uint8_t rfa:3;
    uint8_t PD:1;
    uint32_t SId;
} PACKED;

struct FIG2_Extended_Label {
    uint8_t Rfa:4;
    uint8_t SegmentCount:3;
    uint8_t EncodingFlag:1;

    uint16_t CharacterFlag;
} PACKED;

#ifdef _WIN32
#   pragma pack(pop)
#endif

} // namespace FIC

#endif // __FIG2_H_

