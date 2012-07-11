/*
   Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Her Majesty the Queen in
   Right of Canada (Communications Research Center Canada)
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

#ifndef DAB_INPUT_H
#define DAB_INPUT_H

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif
#include "TcpLog.h"
extern TcpLog etiLog;


struct dabInputOperations {
    int (*init)(void** args);
    int (*open)(void* args, const char* name);
    int (*setbuf)(void* args, int size);
    int (*read)(void* args, void* buffer, int size);
    int (*lock)(void* args);
    int (*unlock)(void* args);
    int (*readFrame)(dabInputOperations* ops, void* args, void* buffer, int size);
    int (*setBitrate)(dabInputOperations* ops, void* args, int bitrate);
    int (*close)(void* args);
    int (*clean)(void** args);
    int (*rewind)(void* args);
    bool operator==(const dabInputOperations&);
};


int dabInputSetbuf(void* args, int size);
int dabInputSetbitrate(dabInputOperations* ops, void* args, int bitrate);


#endif // DAB_INPUT_H
