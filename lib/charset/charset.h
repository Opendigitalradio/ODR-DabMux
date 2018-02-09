/*
    Copyright (C) 2015 Matthias P. Braendli (http://opendigitalradio.org)

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
    \brief Define the EBU charset according to ETSI TS 101 756v1.8.1 for DLS encoding

    \author Matthias P. Braendli <matthias@mpb.li>
    \author Lindsay Cornell
*/

#ifndef __CHARSET_H_
#define __CHARSET_H_

#include "utf8.h"
#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>

#define CHARSET_TABLE_OFFSET 1 // NUL at index 0 cannot be represented
#define CHARSET_TABLE_ENTRIES (256 - CHARSET_TABLE_OFFSET)
extern const char* utf8_encoded_EBU_Latin[CHARSET_TABLE_ENTRIES];

class CharsetConverter
{
    public:
        CharsetConverter() {
            /*! Build the converstion table that contains the known code points,
             * at the indices corresponding to the EBU Latin table
             */
            using namespace std;
            for (size_t i = 0; i < CHARSET_TABLE_ENTRIES; i++) {
                string table_entry(utf8_encoded_EBU_Latin[i]);
                string::iterator it = table_entry.begin();
                uint32_t code_point = utf8::next(it, table_entry.end());
                m_conversion_table.push_back(code_point);
            }
        }

        /*! Convert a UTF-8 encoded text line into an EBU Latin encoded byte stream
         */
        std::string convert(std::string line_utf8) {
            using namespace std;

            // check for invalid utf-8, we only convert up to the first error
            string::iterator end_it = utf8::find_invalid(line_utf8.begin(), line_utf8.end());

            // Convert it to utf-32
            vector<uint32_t> utf32line;
            utf8::utf8to32(line_utf8.begin(), end_it, back_inserter(utf32line));

            string encoded_line(utf32line.size(), '0');

            // Try to convert each codepoint
            for (size_t i = 0; i < utf32line.size(); i++) {
                vector<uint32_t>::iterator iter = find(m_conversion_table.begin(),
                        m_conversion_table.end(), utf32line[i]);
                if (iter != m_conversion_table.end()) {
                    size_t index = std::distance(m_conversion_table.begin(), iter);

                    encoded_line[i] = (char)(index + CHARSET_TABLE_OFFSET);
                }
                else {
                    encoded_line[i] = ' ';
                }
            }
            return encoded_line;
        }

    private:

        std::vector<uint32_t> m_conversion_table;
};

#endif
