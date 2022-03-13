/*
    Copyright (C) 2018 Matthias P. Braendli (http://opendigitalradio.org)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
/*!
    \file charset.h
    \brief A converter for UTF-8 to EBU Latin charset according to
           ETSI TS 101 756 Annex C, used for DLS and Labels.

    \author Matthias P. Braendli
    \author Lindsay Cornell
*/

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "utf8.h"

class CharsetConverter
{
    public:
        CharsetConverter();

        /*! Convert a UTF-8 encoded text line into an EBU Latin encoded byte
         *  stream. If up_to_first_error is set, convert as much text as possible.
         *  If false, raise an utf8::exception in case of conversion errors.
         */
        std::string utf8_to_ebu(std::string line_utf8, bool up_to_first_error = true);

        /*! Convert a EBU Latin byte stream to a UTF-8 encoded string.
         *  Invalid input characters are converted to ⁇ (unicode U+2047).
         */
        std::string ebu_to_utf8(const std::string& str);

    private:
        // Representation of the table in 32-bit unicode
        std::vector<uint32_t> m_conversion_table;
};
