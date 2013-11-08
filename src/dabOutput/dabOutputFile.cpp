/*
   Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Her Majesty the Queen in
   Right of Canada (Communications Research Center Canada)

   Copyright (C) 2013 Matthias P. Braendli
   http://mpb.li

   File output
   */
/*
   This file is part of CRC-DabMux.

   CRC-DabMux is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   CRC-DabMux is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with CRC-DabMux.  If not, see <http://www.gnu.org/licenses/>.
   */
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <limits.h>
#include "dabOutput.h"

int DabOutputFile::Open(const char* filename)
{
    char* token = strchr((char*)filename, '?');
    if (token != NULL) {
        *(token++) = 0;
        char* nextPair;
        char* key;
        char* value;
        // Go through all the &stuff=foo pairs
        // Only the key "type" is supported
        do {
            nextPair = strchr(token, '&');
            if (nextPair != NULL) {
                *nextPair = 0;
            }
            key = token;
            value = strchr(token, '=');
            if (value != NULL) {
                *(value++) = 0;
                if (strcmp(key, "type") == 0) {
                    if (strcmp(value, "raw") == 0) {
                        this->type_ = ETI_FILE_TYPE_RAW;
                        break;
                    } else if (strcmp(value, "framed") == 0) {
                        this->type_ = ETI_FILE_TYPE_FRAMED;
                        break;
                    } else if (strcmp(value, "streamed") == 0) {
                        this->type_ = ETI_FILE_TYPE_STREAMED;
                        break;
                    } else {
                        etiLog.printHeader(TcpLog::ERR,
                                "File type '%s' is not supported.\n", value);
                        return -1;
                    }
                }
                else {
                    etiLog.printHeader(TcpLog::WARNING, "Parameter '%s' unknown\n", key);
                }
            }
        } while (nextPair != NULL);
    }

    this->file_ = open(filename, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0666);
    if (this->file_ == -1) {
        perror(filename);
        return -1;
    }
    return 0;
}

int DabOutputFile::Write(void* buffer, int size)
{
    uint8_t padding[6144];
    ++nbFrames_;

    switch (this->type_) {
    case ETI_FILE_TYPE_FRAMED:
        // Writting nb of frames at beginning of file
        if (lseek(this->file_, 0, SEEK_SET) == -1) goto FILE_WRITE_ERROR;
        if (write(this->file_, &this->nbFrames_, 4) == -1) goto FILE_WRITE_ERROR;

        // Writting nb frame length at end of file
        if (lseek(this->file_, 0, SEEK_END) == -1) goto FILE_WRITE_ERROR;
        if (write(this->file_, &size, 2) == -1) goto FILE_WRITE_ERROR;

        // Appending data
        if (write(this->file_, buffer, size) == -1) goto FILE_WRITE_ERROR;
        break;
    case ETI_FILE_TYPE_STREAMED:
        // Writting nb frame length at end of file
        if (write(this->file_, &size, 2) == -1) goto FILE_WRITE_ERROR;

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
    default:
        etiLog.printHeader(TcpLog::ERR, "File type is not supported.\n");
        return -1;
    }

    return size;

FILE_WRITE_ERROR:
    perror("Error while writting to file");
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

