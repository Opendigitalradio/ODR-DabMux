/*
   Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Her Majesty the Queen in
   Right of Canada (Communications Research Center Canada)

   Copyright (C) 2016
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://www.opendigitalradio.org

   File output
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
#include <string>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <limits.h>
#include "dabOutput.h"

int DabOutputFile::Open(const char* filename)
{
    filename_ = SetEtiType(filename);

    this->file_ = open(filename_.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0666);
    if (this->file_ == -1) {
        perror(filename_.c_str());
        return -1;
    }
    return 0;
}

int DabOutputFile::Write(void* buffer, int size)
{
    uint8_t padding[6144];
    const uint16_t frame_size = size;
    ++nbFrames_;

    switch (this->type_) {
    case ETI_FILE_TYPE_FRAMED:
        // Writing nb of frames at beginning of file
        if (lseek(this->file_, 0, SEEK_SET) == -1) goto FILE_WRITE_ERROR;
        if (write(this->file_, &this->nbFrames_, 4) == -1) goto FILE_WRITE_ERROR;

        // Writing nb frame length at end of file
        if (lseek(this->file_, 0, SEEK_END) == -1) goto FILE_WRITE_ERROR;
        if (write(this->file_, &frame_size, 2) == -1) goto FILE_WRITE_ERROR;

        // Appending data
        if (write(this->file_, buffer, size) == -1) goto FILE_WRITE_ERROR;
        break;
    case ETI_FILE_TYPE_STREAMED:
        // Writing nb frame length at end of file
        if (write(this->file_, &frame_size, 2) == -1) goto FILE_WRITE_ERROR;

        // Appending data
        if (write(this->file_, buffer, size) == -1) goto FILE_WRITE_ERROR;
        break;
    case ETI_FILE_TYPE_RAW:
        // Appending data
        if (write(this->file_, buffer, size) == -1) goto FILE_WRITE_ERROR;

        // Appending padding
        memset(padding, 0x55, 6144 - size);
        if (write(this->file_, padding, 6144 - size) == -1) goto FILE_WRITE_ERROR;
        break;
    case ETI_FILE_TYPE_NONE:
    default:
        etiLog.log(error, "File type is not supported.\n");
        return -1;
    }

    return size;

FILE_WRITE_ERROR:
    perror("Error while writing to file");
    return -1;
}

int DabOutputFile::Close()
{
    if (close(this->file_) == 0) {
        this->file_ = -1;
        return 0;
    }
    perror("Can't close file");
    return -1;
}

std::string DabOutputFile::SetEtiType(const std::string& filename)
{
    size_t ix = filename.find('?');
    const std::string filename_before_q = filename.substr(0, ix);

    if (ix != std::string::npos) {

        do {
            const size_t ix_key = ix + 1;

            const size_t ix_eq = filename.find('=', ix);
            if (ix_eq == std::string::npos) {
                // no equals sign, not a valid query. Return up to the question mark
                break;
            }

            const size_t ix_val = ix_eq + 1;

            const std::string key = filename.substr(ix_key, ix_eq - ix_key);

            ix = filename.find('&', ix);
            const size_t len_value = (ix == std::string::npos) ? std::string::npos : ix - ix_val;

            if (key == "type") {
                const std::string value = filename.substr(ix_val, len_value);

                if (value == "raw") {
                    this->type_ = ETI_FILE_TYPE_RAW;
                    break;
                } else if (value == "framed") {
                    this->type_ = ETI_FILE_TYPE_FRAMED;
                    break;
                } else if (value == "streamed") {
                    this->type_ = ETI_FILE_TYPE_STREAMED;
                    break;
                } else {
                    std::stringstream ss;
                    ss << "File type '" << value << "' is not supported.";
                    throw std::runtime_error(ss.str());
                }
            }

        }
        while (ix != std::string::npos);
    }

    return filename_before_q;
}

