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

#ifndef _DABOUTPUT
#define _DABOUTPUT

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif


struct dabOutputOperations {
    int (*init)(void** args);
    int (*open)(void* args, const char* name);
    int (*write)(void* args, void* buffer, int size);
    int (*close)(void* args);
    int (*clean)(void** args);
};

extern struct dabOutputOperations dabOutputDefaultOperations;

#if defined(HAVE_OUTPUT_FILE)
extern struct dabOutputOperations dabOutputFileOperations;
#endif // defined(HAVE_OUTPUT_FILE)

#if defined(HAVE_OUTPUT_FIFO)
extern struct dabOutputOperations dabOutputFifoOperations;
#endif // defined(HAVE_OUTPUT_FIFO)

#if defined(HAVE_OUTPUT_UDP)
extern struct dabOutputOperations dabOutputUdpOperations;
#endif // defined(HAVE_OUTPUT_UDP)

#if defined(HAVE_OUTPUT_TCP)
extern struct dabOutputOperations dabOutputTcpOperations;
#endif // defined(HAVE_OUTPUT_TCP)

#if defined(HAVE_OUTPUT_RAW)
extern struct dabOutputOperations dabOutputRawOperations;
#endif // defined(HAVE_OUTPUT_RAW)

#if defined(HAVE_OUTPUT_SIMUL)
extern struct dabOutputOperations dabOutputSimulOperations;
#endif // defined(HAVE_OUTPUT_SIMUL)


extern const unsigned char revTable[];


#include "TcpLog.h"
extern TcpLog etiLog;


#endif // _DABOUTPUT
