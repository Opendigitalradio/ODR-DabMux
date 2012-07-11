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

#include "dabOutput.h"
#include "UdpSocket.h"

#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <limits.h>

#ifdef _WIN32
#   include <io.h>
#   ifdef __MINGW32__
#       define FS_DECLARE_CFG_ARRAYS
#       include <winioctl.h>
#   endif
#   include <sdci.h>
#else
#   include <farsync.h>
#   include <unistd.h>
#   include <sys/time.h>
#   ifndef O_BINARY
#       define O_BINARY 0
#   endif // O_BINARY
#endif


int dabOutputDefaultInit(void** args) {
    return -1;
}


int dabOutputDefaultOpen(void* args, const char* filename)
{
    return -1;
}


int dabOutputDefaultWrite(void* args, void* buffer, int size)
{
    return -1;
}


int dabOutputDefaultClose(void* args)
{
    return -1;
}


int dabOutputDefaultClean(void** args)
{
    return -1;
}


struct dabOutputOperations dabOutputDefaultOperations  = {
    dabOutputDefaultInit,
    dabOutputDefaultOpen,
    dabOutputDefaultWrite,
    dabOutputDefaultClose,
    dabOutputDefaultClean
};


enum EtiFileType {
    ETI_FILE_TYPE_NONE = 0,
    ETI_FILE_TYPE_RAW,
    ETI_FILE_TYPE_STREAMED,
    ETI_FILE_TYPE_FRAMED
};


struct dabOutputFifoData {
    int file;
    EtiFileType type;
    unsigned long nbFrames;
};


int dabOutputFifoInit(void** args)
{
    dabOutputFifoData* data = new dabOutputFifoData;

    *args = data;
    data->file = -1;
    data->type = ETI_FILE_TYPE_STREAMED;
    data->nbFrames = 0;
    return 0;
}


int dabOutputFifoOpen(void* args, const char* filename)
{
    dabOutputFifoData* data = (dabOutputFifoData*)args;

    char* token = strchr((char*)filename, '?');
    if (token != NULL) {
        *(token++) = 0;
        char* nextPair;
        char* key;
        char* value;
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
                        data->type = ETI_FILE_TYPE_RAW;
                        break;
                    } else if (strcmp(value, "framed") == 0) {
                        data->type = ETI_FILE_TYPE_FRAMED;
                        break;
                    } else if (strcmp(value, "streamed") == 0) {
                        data->type = ETI_FILE_TYPE_STREAMED;
                        break;
                    } else {
                        etiLog.printHeader(TcpLog::ERR,
                                "File type '%s' is not supported.\n", value);
                        return -1;
                    }
                }
            }
        } while (nextPair != NULL);
    }

    data->file = open(filename, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0666);
    if (data->file == -1) {
        perror(filename);
        return -1;
    }
    return 0;
}


int dabOutputFifoWrite(void* args, void* buffer, int size)
{
    dabOutputFifoData* data = (dabOutputFifoData*)args;
    uint8_t padding[6144];

    switch (data->type) {
    case ETI_FILE_TYPE_FRAMED:
        if (data->nbFrames == 0) {
            uint32_t nbFrames = (uint32_t)-1;
            // Writting nb frame
            if (write(data->file, &nbFrames, 4) == -1) goto FIFO_WRITE_ERROR;
        }
    case ETI_FILE_TYPE_STREAMED:
        // Writting frame length
        if (write(data->file, &size, 2) == -1) goto FIFO_WRITE_ERROR;
        // Appending data
        if (write(data->file, buffer, size) == -1) goto FIFO_WRITE_ERROR;
        break;
    case ETI_FILE_TYPE_RAW:
        // Appending data
        if (write(data->file, buffer, size) == -1) goto FIFO_WRITE_ERROR;
        // Appending padding
        memset(padding, 0x55, 6144 - size);
        if (write(data->file, padding, 6144 - size) == -1) goto FIFO_WRITE_ERROR;
        break;
    default:
        etiLog.printHeader(TcpLog::ERR, "File type is not supported.\n");
        return -1;
    }


    return size;

FIFO_WRITE_ERROR:
    perror("Error while writting to file");
    return -1;
}


int dabOutputFifoClose(void* args)
{
    dabOutputFifoData* data = (dabOutputFifoData*)args;

    if (close(data->file) == 0) {
        data->file = -1;
        return 0;
    }
    perror("Can't close file");
    return -1;
}


int dabOutputFifoClean(void** args)
{
    delete (dabOutputFifoData*)(*args);
    return 0;
}


struct dabOutputOperations dabOutputFifoOperations  = {
    dabOutputFifoInit,
    dabOutputFifoOpen,
    dabOutputFifoWrite,
    dabOutputFifoClose,
    dabOutputFifoClean
};


struct dabOutputFileData {
    int file;
    EtiFileType type;
    unsigned long nbFrames;
};


int dabOutputFileInit(void** args)
{
    dabOutputFileData* data = new dabOutputFileData;

    *args = data;
    data->file = -1;
    data->type = ETI_FILE_TYPE_FRAMED;
    data->nbFrames = 0;
    return 0;
}


int dabOutputFileWrite(void* args, void* buffer, int size)
{
    dabOutputFileData* data = (dabOutputFileData*)args;

    uint8_t padding[6144];
    ++data->nbFrames;

    switch (data->type) {
    case ETI_FILE_TYPE_FRAMED:
        // Writting nb of frames at beginning of file
        if (lseek(data->file, 0, SEEK_SET) == -1) goto FILE_WRITE_ERROR;
        if (write(data->file, &data->nbFrames, 4) == -1) goto FILE_WRITE_ERROR;

        // Writting nb frame length at end of file
        if (lseek(data->file, 0, SEEK_END) == -1) goto FILE_WRITE_ERROR;
        if (write(data->file, &size, 2) == -1) goto FILE_WRITE_ERROR;

        // Appending data
        if (write(data->file, buffer, size) == -1) goto FILE_WRITE_ERROR;
        break;
    case ETI_FILE_TYPE_STREAMED:
        // Writting nb frame length at end of file
        if (write(data->file, &size, 2) == -1) goto FILE_WRITE_ERROR;

        // Appending data
        if (write(data->file, buffer, size) == -1) goto FILE_WRITE_ERROR;
        break;
    case ETI_FILE_TYPE_RAW:
        // Appending data
        if (write(data->file, buffer, size) == -1) goto FILE_WRITE_ERROR;

        // Appending padding
        memset(padding, 0x55, 6144 - size);
        if (write(data->file, padding, 6144 - size) == -1) goto FILE_WRITE_ERROR;
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


int dabOutputFileClean(void** args)
{
    delete (dabOutputFileData*)(*args);
    return 0;
}


struct dabOutputOperations dabOutputFileOperations  = {
    dabOutputFileInit,
    dabOutputFifoOpen,
    dabOutputFileWrite,
    dabOutputFifoClose,
    dabOutputFileClean
};


struct dabOutputRawData {
#ifdef _WIN32
    HANDLE socket;
#else
    int socket;
    bool isCyclades;
#endif
    unsigned char* buffer;
};


const unsigned char revTable[] = {
    0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
    0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
    0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
    0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
    0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
    0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
    0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
    0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
    0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
    0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
    0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
    0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
    0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
    0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
    0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
    0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
    0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
    0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
    0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
    0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
    0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
    0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
    0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
    0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
    0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
    0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
    0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
    0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
    0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
    0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
    0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
    0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
};


int dabOutputRawInit(void** args)
{
    dabOutputRawData* data = new dabOutputRawData;

    *args = data;
#ifdef _WIN32
    data->socket = INVALID_HANDLE_VALUE;
#else
    data->socket = -1;
    data->isCyclades = false;
#endif
    data->buffer = new unsigned char[6144];
    return 0;
}


#ifdef _WIN32
#   include <fscfg.h>
#   include <sdci.h>
#else
#   include <netinet/in.h>
#   include <sys/types.h>
#   include <sys/socket.h>
#   include <sys/ioctl.h>
#   include <linux/if_packet.h>
#   include <linux/netdevice.h>
#   include <net/if_arp.h>
#endif
int dabOutputRawOpen(void* args, const char* filename)
{
    dabOutputRawData* data = (dabOutputRawData*)args;

    if (filename == NULL) {
        etiLog.printHeader(TcpLog::ERR, "Socket name must be provided!\n");
        return -1;
    }

#ifdef _WIN32
    // Opening device
    data->socket = CreateFile(filename, GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, NULL);
    if (data->socket == INVALID_HANDLE_VALUE) {
        etiLog.printHeader(TcpLog::ERR, "Can't open raw device '%s': %i\n",
            filename, GetLastError());
        return -1;
    }

    // Configuring device
    DWORD result;
    FS_TE1_CONFIG config;
    if (!DeviceIoControl(data->socket, IoctlCodeFarSyncGetTE1Config, NULL, 0,
                &config, sizeof(config), &result, NULL)) {
        etiLog.printHeader(TcpLog::ERR,
                "Can't get raw device '%s' config: %i\n",
                filename, GetLastError());
        return -1;
    }
    config.dataRate = 2048000;
    config.clocking = CLOCKING_MASTER;
    config.framing = FRAMING_E1;
    config.structure = STRUCTURE_UNFRAMED;
    config.iface = INTERFACE_BNC;
    config.coding = CODING_HDB3;
    config.lineBuildOut = LBO_0dB;
    config.equalizer = EQUALIZER_SHORT;
    config.transparentMode = TRUE;
    config.loopMode = LOOP_NONE;
    config.range = RANGE_0_40_M;
    config.txBufferMode = BUFFER_2_FRAME;
    config.rxBufferMode = BUFFER_2_FRAME;
    config.startingTimeSlot = 0;
    config.losThreshold = 2;
    config.enableIdleCode = TRUE;
    config.idleCode = 0xff;
    if (!DeviceIoControl(data->socket, IoctlCodeFarSyncSetTE1Config,
                &config, sizeof(config), NULL, 0, &result, NULL)) {
        etiLog.printHeader(TcpLog::ERR,
                "Can't set raw device '%s' config: %i\n",
                filename, GetLastError());
        return -1;
    }

    // Starting device
    if (!DeviceIoControl(data->socket, IoctlCodeFarSyncQuickStart, NULL, 0,
                NULL, 0, &result, NULL)) {
        etiLog.printHeader(TcpLog::ERR, "Can't start raw device '%s': %i\n",
                filename, GetLastError());
        return -1;
    }
#else
    data->socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (data->socket == -1) {
        etiLog.printHeader(TcpLog::ERR, "Are you logged as root?\n");
        perror(filename);
        return -1;
    }

    struct ifreq ifr;
    struct sockaddr_ll saddr;
    memset(&ifr, 0, sizeof(struct ifreq));
    (void)strncpy(ifr.ifr_name, filename, sizeof(ifr.ifr_name));

    // Get current Farsync configuration
    struct fstioc_info info;
    memset(&info, 0, sizeof(info));
    ifr.ifr_data = (char*)&info;
    if (ioctl(data->socket, FSTGETCONF, &ifr) == -1) {
        etiLog.printHeader(TcpLog::DBG, "Cyclades card identified.\n");
        data->isCyclades = true;

        // Set the interface MTU if needed
        if (ioctl(data->socket, SIOCGIFMTU, &ifr) == -1) {
            etiLog.printHeader(TcpLog::ERR, "Can't get raw device MTU!\n");
            perror(filename);
            return -1;
        } else {
            if (ifr.ifr_mtu != 6143) {
                ifr.ifr_mtu = 6143;
                if (ioctl(data->socket, SIOCSIFMTU, &ifr) == -1) {
                    etiLog.printHeader(TcpLog::ERR,
                            "Can't Cyclades device MTU!\n");
                    perror(filename);
                    return -1;
                }
            }
        }
    } else {
        etiLog.printHeader(TcpLog::DBG, "Farsync card identified.\n");
        data->isCyclades = false;

        info.lineInterface = E1;
        info.proto = FST_RAW;
        info.internalClock = EXTCLK;
        info.lineSpeed = 2048000;
        //info.debug = DBG_INIT | DBG_OPEN | DBG_PCI | DBG_IOCTL | DBG_TX;
        info.transparentMode = 1;
        info.ignoreCarrier = 1;
        info.numTxBuffers = 8;
        info.numRxBuffers = 8;
        info.txBufferSize = 6144;
        info.rxBufferSize = 6144;
        // E1 specific config
        info.clockSource = CLOCKING_SLAVE;
        info.structure = STRUCTURE_UNFRAMED;
        info.interface = INTERFACE_BNC; //RJ48C;
        info.coding = CODING_HDB3;
        info.txBufferMode = BUFFER_2_FRAME;
        info.idleCode = 0xff;
        info.valid = FSTVAL_ALL;

        // Setting configuration
        etiLog.printHeader(TcpLog::DBG, "Set configuration.\n");
        ifr.ifr_data = (char*)&info;
        if (ioctl(data->socket, FSTSETCONF, &ifr) == -1) {
            etiLog.printHeader(TcpLog::ERR,
                    "Can't set Farsync configurationi!\n");
            perror(filename);
            return -1;
        }

        // Disabling notify
        etiLog.printHeader(TcpLog::DBG, "Disable notify.\n");
        int notify = 0;
        ifr.ifr_data = (char*)&notify;
        if (ioctl(data->socket, FSTSNOTIFY, &ifr) == -1) {
            etiLog.printHeader(TcpLog::ERR, "Can't disable Farsync notify!\n");
            perror(filename);
            return -1;
        }

        // Apply the new configuration
        // Set the interface down if needed
        etiLog.printHeader(TcpLog::DBG, "Get flags.\n");
        if (ioctl(data->socket, SIOCGIFFLAGS, &ifr) == -1) {
            etiLog.printHeader(TcpLog::ERR, "Can't get Farsync flags!\n");
            perror(filename);
            return -1;
        } else {
            if (ifr.ifr_flags & IFF_UP) {
                etiLog.printHeader(TcpLog::DBG, "Set flags.\n");
                ifr.ifr_flags &= ~IFF_UP;
                if (ioctl(data->socket, SIOCSIFFLAGS, &ifr) == -1) {
                    etiLog.printHeader(TcpLog::ERR,
                            "Can't turn down Farsync device!\n");
                    perror(filename);
                    return -1;
                }
            }
        }

        // Set the interface MTU if needed
        etiLog.printHeader(TcpLog::DBG, "Get MTU.\n");
        if (ioctl(data->socket, SIOCGIFMTU, &ifr) == -1) {
            etiLog.printHeader(TcpLog::ERR, "Can't get Farsync MTU!\n");
            perror(filename);
            return -1;
        } else {
            if (ifr.ifr_mtu != 6144) {
                etiLog.printHeader(TcpLog::DBG, "Set MTU.\n");
                ifr.ifr_mtu = 6144;
                if (ioctl(data->socket, SIOCSIFMTU, &ifr) == -1) {
                    etiLog.printHeader(TcpLog::ERR, "Can't set Farsync MTU!\n");
                    perror(filename);
                    return -1;
                }
            }
        }
    }

    // Set the interface up if needed
    etiLog.printHeader(TcpLog::DBG, "Get flags.\n");
    if (ioctl(data->socket, SIOCGIFFLAGS, &ifr) == -1) {
        etiLog.printHeader(TcpLog::ERR, "Can't get raw device flags!\n");
        perror(filename);
        return -1;
    } else {
        if (!(ifr.ifr_flags & IFF_UP)) {
            ifr.ifr_flags |= IFF_UP;
            etiLog.printHeader(TcpLog::DBG, "Set flags.\n");
            if (ioctl(data->socket, SIOCSIFFLAGS, &ifr) == -1) {
                etiLog.printHeader(TcpLog::ERR, "Can't turn up raw device!\n");
                perror(filename);
                return -1;
            }
        }
    }

    close(data->socket);

    ////////////////////
    // Opening device //
    ////////////////////

    if ((data->socket = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_CUST))) == -1) {
        etiLog.printHeader(TcpLog::ERR, "Are you logged as root?\n");
        perror(filename);
        return -1;
    }

    // ioctl to read the interface number
    etiLog.printHeader(TcpLog::DBG, "Get index.\n");
    memset(&ifr, 0, sizeof(struct ifreq));
    strncpy(ifr.ifr_name, filename, sizeof(ifr.ifr_name));
    if (ioctl(data->socket, SIOCGIFINDEX, (char *) &ifr) == -1) {
        perror(filename);
        return -1;
    }

    // Bind to the interface name
    etiLog.printHeader(TcpLog::DBG, "Bind interface.\n");
    memset(&saddr, 0, sizeof(struct sockaddr_ll));
    saddr.sll_family = AF_PACKET;
    saddr.sll_protocol = ARPHRD_RAWHDLC;
    saddr.sll_ifindex = ifr.ifr_ifindex;
    if (bind(data->socket, (struct sockaddr *) &saddr, sizeof(saddr)) == -1) {
        etiLog.printHeader(TcpLog::ERR, "Can't bind raw device!\n");
        perror(filename);
        return -1;
    }
#endif

    return 0;
}


int dabOutputRawWrite(void* args, void* buffer, int size)
{
    dabOutputRawData* data = (dabOutputRawData*)args;

    // Encoding data
    memcpy(data->buffer, buffer, size);
    memset(data->buffer + size, 0x55, 6144 - size);
    for (int i = 0; i < 6144; ++i) {
        data->buffer[i] = revTable[data->buffer[i]];
    }

    // Writting data
#ifdef _WIN32
    DWORD result;
    if(!DeviceIoControl(data->socket, IoctlCodeTxFrame, data->buffer, 6144,
        NULL, 0, &result, NULL)) {
            goto RAW_WRITE_ERROR;
    }
#else
    /*
    if (write(data->socket, data->buffer + 1, 6143) != 6143) {
        goto RAW_WRITE_ERROR;
    }
    */
    if (data->isCyclades) {
        if (write(data->socket, data->buffer + 1, 6143) != 6143) {
            goto RAW_WRITE_ERROR;
        }
    } else {
        int ret = send(data->socket, data->buffer, 6144, 0);
        if (ret != 6144) {
            fprintf(stderr, "%i/6144 bytes written\n", ret);
            goto RAW_WRITE_ERROR;
        }
    }
#endif

    return size;

RAW_WRITE_ERROR:
#ifdef _WIN32
    DWORD err = GetLastError();
    LPSTR errMsg;
    if(FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        NULL,
        err,
        0,
        (LPTSTR)&errMsg,
        0,
        NULL) == 0) {
            fprintf(stderr, "Error while writting to raw socket: %i\n", err);
    } else {
        fprintf(stderr, "Error while writting to raw socket: %s\n", errMsg);
        LocalFree(errMsg);
    }
#else
    perror("Error while writting to raw socket");
#endif

    return -1;
}


int dabOutputRawClose(void* args)
{
    dabOutputRawData* data = (dabOutputRawData*)args;

#ifdef _WIN32
    CancelIo(data->socket);
    CloseHandle(data->socket);
    return 0;
#else
    if (close(data->socket) == 0) {
        data->socket = -1;
        return 0;
    }
    perror("Can't close raw socket");
#endif

    return -1;
}


int dabOutputRawClean(void** args)
{
    dabOutputRawData* data = *((dabOutputRawData**)args);

    delete []data->buffer;
    delete data;
    return 0;
}


struct dabOutputOperations dabOutputRawOperations  = {
    dabOutputRawInit,
    dabOutputRawOpen,
    dabOutputRawWrite,
    dabOutputRawClose,
    dabOutputRawClean
};


struct dabOutputUdpData {
    UdpSocket* socket;
    UdpPacket* packet;
};


int dabOutputUdpInit(void** args)
{
    dabOutputUdpData* data = new dabOutputUdpData;

    *args = data;
    UdpSocket::init();
    data->packet = new UdpPacket(6144);
    data->socket = new UdpSocket();

    return 0;
}


int dabOutputUdpOpen(void* args, const char* filename)
{
    dabOutputUdpData* data = (dabOutputUdpData*)args;
    filename = strdup(filename);

    char* address;
    long port;
    address = strchr((char*)filename, ':');
    if (address == NULL) {
        etiLog.printHeader(TcpLog::ERR,
                "\"%s\" is an invalid format for udp address: "
                "should be [address]:port - > aborting\n",
                filename);
        return -1;
    }
    *(address++) = 0;
    port = strtol(address, (char **)NULL, 10);
    if ((port == LONG_MIN) || (port == LONG_MAX)) {
        etiLog.printHeader(TcpLog::ERR,
                "can't convert port number in udp address %s\n", address);
        return -1;
    }
    if (port == 0) {
        etiLog.printHeader(TcpLog::ERR,
                "can't use port number 0 in udp address\n");
        return -1;
    }
    address = (char*)filename;
    if (strlen(address) > 0) {
        if (data->packet->getAddress().setAddress(address) == -1) {
            etiLog.printHeader(TcpLog::ERR, "can't set address %s (%s: %s) "
                    "-> aborting\n", address, inetErrDesc, inetErrMsg);
            return -1;
        }
    }
    data->packet->getAddress().setPort(port);

    if (data->socket->create() == -1) {
        etiLog.printHeader(TcpLog::ERR, "can't create Udp socket (%s: %s) "
                "-> aborting\n)", inetErrDesc, inetErrMsg);
        return -1;
    }

    //sprintf(filename, "%s:%i", data->packet->getAddress().getHostAddress(),
    //        data->packet->getAddress().getPort());
    return 0;
}


int dabOutputUdpWrite(void* args, void* buffer, int size)
{
    dabOutputUdpData* data = (dabOutputUdpData*)args;

    data->packet->setLength(0);
    data->packet->addData(buffer, size);
    return data->socket->send(*data->packet);
}


int dabOutputUdpClose(void* args)
{
    //dabOutputUdpData* data = (dabOutputUdpData*)args;

    return 0;
}


int dabOutputUdpClean(void** args)
{
    dabOutputUdpData* data = *((dabOutputUdpData**)args);

    delete data->socket;
    delete data->packet;
    delete data;

    return 0;
}


struct dabOutputOperations dabOutputUdpOperations  = {
    dabOutputUdpInit,
    dabOutputUdpOpen,
    dabOutputUdpWrite,
    dabOutputUdpClose,
    dabOutputUdpClean
};


#include "TcpServer.h"
struct dabOutputTcpData {
    TcpServer* server;
    TcpSocket* client;
    pthread_t thread;
};


int dabOutputTcpInit(void** args)
{
    dabOutputTcpData* data = new dabOutputTcpData;

    *args = data;
    TcpSocket::init();
    data->server = new TcpServer();
    data->client = NULL;

    return 0;
}


void* dabOutputTcpThread(void* args)
{
    dabOutputTcpData* data = (dabOutputTcpData*)args;
    TcpSocket* client;

    while ((client = data->server->accept()) != NULL) {
        etiLog.print(TcpLog::INFO, "TCP server got a new client.\n");
        if (data->client != NULL) {
            delete data->client;
        }
        data->client = client;
    }
    etiLog.print(TcpLog::ERR, "TCP thread can't accept new client (%s)\n",
            inetErrDesc, inetErrMsg);

    return NULL;
}


int dabOutputTcpOpen(void* args, const char* filename)
{
    dabOutputTcpData* data = (dabOutputTcpData*)args;
    filename = strdup(filename);

    char* address;
    long port;
    address = strchr((char*)filename, ':');
    if (address == NULL) {
        etiLog.printHeader(TcpLog::ERR,
                "\"%s\" is an invalid format for tcp address: "
                "should be [address]:port - > aborting\n",
                filename);
        return -1;
    }
    *(address++) = 0;
    port = strtol(address, (char **)NULL, 10);
    if ((port == LONG_MIN) || (port == LONG_MAX)) {
        etiLog.printHeader(TcpLog::ERR,
                "can't convert port number in tcp address %s\n", address);
        return -1;
    }
    if (port == 0) {
        etiLog.printHeader(TcpLog::ERR,
                "can't use port number 0 in tcp address\n");
        return -1;
    }
    address = (char*)filename;
    if (strlen(address) > 0) {
        if (data->server->create(port, address) == -1) {
            etiLog.printHeader(TcpLog::ERR, "Can't create Tcp server on %s:%i "
                    "(%s: %s) -> aborting\n",
                    address, port, inetErrDesc, inetErrMsg);
            return -1;
        }
    } else {
        if (data->server->create(port) == -1) {
            etiLog.printHeader(TcpLog::ERR, "Can't create Tcp server on :%i "
                    "(%s: %s) -> aborting\n",
                    port, inetErrDesc, inetErrMsg);
            return -1;
        }
    }

    //sprintf(filename, "%s:%i", data->packet->getAddress().getHostAddress(),
    //        data->packet->getAddress().getPort());

    if (data->server->listen() == -1) {
        etiLog.printHeader(TcpLog::ERR, "Can't listen on Tcp socket (%s: %s)\n",
                inetErrDesc, inetErrMsg);
        return -1;
    }
#ifdef _WIN32
    data->thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)dabOutputTcpThread, data, 0, NULL);
    if (data->thread == NULL) {
        fprintf(stderr, "Can't create TCP child");
        return -1;
    }
#else
    if (pthread_create(&data->thread, NULL, dabOutputTcpThread, data)) {
        perror("Can't create TCP child");
        return -1;
    }
#endif

    return 0;
}


int dabOutputTcpWrite(void* args, void* buffer, int size)
{
    dabOutputTcpData* data = (dabOutputTcpData*)args;

    if (data->client != NULL) {
        if (data->client->write(&size, 2) == 2) {
            if (data->client->write(buffer, size) != size) {
                return size;
            }
        }
        else {
            etiLog.print(TcpLog::INFO, "TCP server client disconnected.\n");
            delete data->client;
            data->client = NULL;
        }
    }
    return size;
}


#include <signal.h>
int dabOutputTcpClose(void* args)
{
    dabOutputTcpData* data = (dabOutputTcpData*)args;

    data->server->close();
    if( data->client != NULL )
        data->client->close();
#ifdef WIN32
    DWORD status;
    for (int i = 0; i < 5; ++i) {
        if (GetExitCodeThread(data->thread, &status)) {
            break;
        }
        Sleep(100);
    }
    TerminateThread(data->thread, 1);
#else
    pthread_kill(data->thread, SIGPIPE);
#endif

    return 0;
}


int dabOutputTcpClean(void** args)
{
    dabOutputTcpData* data = *((dabOutputTcpData**)args);

#ifdef _WIN32
    CloseHandle(data->thread);
#endif

    delete data->server;
    delete data->client;
    delete data;

    return 0;
}


struct dabOutputOperations dabOutputTcpOperations  = {
    dabOutputTcpInit,
    dabOutputTcpOpen,
    dabOutputTcpWrite,
    dabOutputTcpClose,
    dabOutputTcpClean
};


struct dabOutputSimulData {
#ifdef _WIN32
    DWORD startTime;
#else
    timeval startTime;
#endif
};


int dabOutputSimulInit(void** args) {
    dabOutputSimulData* data = new dabOutputSimulData;

    *args = data;

    return 0;
}


int dabOutputSimulOpen(void* args, const char* filename)
{
    dabOutputSimulData* data = (dabOutputSimulData*)args;

#ifdef _WIN32
    data->startTime = GetTickCount();
#else
    gettimeofday(&data->startTime, NULL);
#endif

    return 0;
}


int dabOutputSimulWrite(void* args, void* buffer, int size)
{
    dabOutputSimulData* data = (dabOutputSimulData*)args;

    unsigned long current;
    unsigned long start;
    unsigned long waiting;

#ifdef _WIN32
    current = GetTickCount();
    start = data->startTime;
    if (current < start) {
        waiting = start - current + 24;
        Sleep(waiting);
    } else {
        waiting = 24 - (current - start);
        if ((current - start) < 24) {
            Sleep(waiting);
        }
    }
    data->startTime += 24;
#else
    timeval curTime;
    gettimeofday(&curTime, NULL);
    current = (1000000ul * curTime.tv_sec) + curTime.tv_usec;
    start = (1000000ul * data->startTime.tv_sec) + data->startTime.tv_usec;
    waiting = 24000ul - (current - start);
    if ((current - start) < 24000ul) {
        usleep(waiting);
    }

    data->startTime.tv_usec += 24000;
    if (data->startTime.tv_usec >= 1000000) {
        data->startTime.tv_usec -= 1000000;
        ++data->startTime.tv_sec;
    }
#endif

    return size;
}


int dabOutputSimulClose(void* args)
{
    //dabOutputSimulData* data = (dabOutputSimulData*)args);

    return 0;
}


int dabOutputSimulClean(void** args)
{
    dabOutputSimulData* data = *((dabOutputSimulData**)args);

    delete data;

    return 0;
}


struct dabOutputOperations dabOutputSimulOperations  = {
    dabOutputSimulInit,
    dabOutputSimulOpen,
    dabOutputSimulWrite,
    dabOutputSimulClose,
    dabOutputSimulClean
};


