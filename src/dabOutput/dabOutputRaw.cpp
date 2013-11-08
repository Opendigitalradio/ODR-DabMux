#include <cstdio>
#include <cstring>
#include "dabOutput.h"
#ifdef _WIN32
#   include <fscfg.h>
#   include <sdci.h>
#else
#   include <farsync.h>
#   include <netinet/in.h>
#   include <sys/types.h>
#   include <sys/socket.h>
#   include <sys/ioctl.h>
#   include <linux/if_packet.h>
#   include <linux/netdevice.h>
#   include <net/if_arp.h>
#endif

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

int DabOutputRaw::Open(const char* filename)
{
    if (filename == NULL) {
        etiLog.printHeader(TcpLog::ERR, "Socket name must be provided!\n");
        return -1;
    }

#ifdef _WIN32
    // Opening device
    this->socket_ = CreateFile(filename, GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, NULL);
    if (this->socket_ == INVALID_HANDLE_VALUE) {
        etiLog.printHeader(TcpLog::ERR, "Can't open raw device '%s': %i\n",
            filename, GetLastError());
        return -1;
    }

    // Configuring device
    DWORD result;
    FS_TE1_CONFIG config;
    if (!DeviceIoControl(this->socket_, IoctlCodeFarSyncGetTE1Config, NULL, 0,
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
    if (!DeviceIoControl(this->socket_, IoctlCodeFarSyncSetTE1Config,
                &config, sizeof(config), NULL, 0, &result, NULL)) {
        etiLog.printHeader(TcpLog::ERR,
                "Can't set raw device '%s' config: %i\n",
                filename, GetLastError());
        return -1;
    }

    // Starting device
    if (!DeviceIoControl(this->socket_, IoctlCodeFarSyncQuickStart, NULL, 0,
                NULL, 0, &result, NULL)) {
        etiLog.printHeader(TcpLog::ERR, "Can't start raw device '%s': %i\n",
                filename, GetLastError());
        return -1;
    }
#else
    this->socket_ = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (this->socket_ == -1) {
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
    if (ioctl(this->socket_, FSTGETCONF, &ifr) == -1) {
        etiLog.printHeader(TcpLog::DBG, "Cyclades card identified.\n");
        this->isCyclades_ = true;

        // Set the interface MTU if needed
        if (ioctl(this->socket_, SIOCGIFMTU, &ifr) == -1) {
            etiLog.printHeader(TcpLog::ERR, "Can't get raw device MTU!\n");
            perror(filename);
            return -1;
        } else {
            if (ifr.ifr_mtu != 6143) {
                ifr.ifr_mtu = 6143;
                if (ioctl(this->socket_, SIOCSIFMTU, &ifr) == -1) {
                    etiLog.printHeader(TcpLog::ERR,
                            "Can't Cyclades device MTU!\n");
                    perror(filename);
                    return -1;
                }
            }
        }
    } else {
        etiLog.printHeader(TcpLog::DBG, "Farsync card identified.\n");
        this->isCyclades_ = false;

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
        if (ioctl(this->socket_, FSTSETCONF, &ifr) == -1) {
            etiLog.printHeader(TcpLog::ERR,
                    "Can't set Farsync configurationi!\n");
            perror(filename);
            return -1;
        }

        // Disabling notify
        etiLog.printHeader(TcpLog::DBG, "Disable notify.\n");
        int notify = 0;
        ifr.ifr_data = (char*)&notify;
        if (ioctl(this->socket_, FSTSNOTIFY, &ifr) == -1) {
            etiLog.printHeader(TcpLog::ERR, "Can't disable Farsync notify!\n");
            perror(filename);
            return -1;
        }

        // Apply the new configuration
        // Set the interface down if needed
        etiLog.printHeader(TcpLog::DBG, "Get flags.\n");
        if (ioctl(this->socket_, SIOCGIFFLAGS, &ifr) == -1) {
            etiLog.printHeader(TcpLog::ERR, "Can't get Farsync flags!\n");
            perror(filename);
            return -1;
        } else {
            if (ifr.ifr_flags & IFF_UP) {
                etiLog.printHeader(TcpLog::DBG, "Set flags.\n");
                ifr.ifr_flags &= ~IFF_UP;
                if (ioctl(this->socket_, SIOCSIFFLAGS, &ifr) == -1) {
                    etiLog.printHeader(TcpLog::ERR,
                            "Can't turn down Farsync device!\n");
                    perror(filename);
                    return -1;
                }
            }
        }

        // Set the interface MTU if needed
        etiLog.printHeader(TcpLog::DBG, "Get MTU.\n");
        if (ioctl(this->socket_, SIOCGIFMTU, &ifr) == -1) {
            etiLog.printHeader(TcpLog::ERR, "Can't get Farsync MTU!\n");
            perror(filename);
            return -1;
        } else {
            if (ifr.ifr_mtu != 6144) {
                etiLog.printHeader(TcpLog::DBG, "Set MTU.\n");
                ifr.ifr_mtu = 6144;
                if (ioctl(this->socket_, SIOCSIFMTU, &ifr) == -1) {
                    etiLog.printHeader(TcpLog::ERR, "Can't set Farsync MTU!\n");
                    perror(filename);
                    return -1;
                }
            }
        }
    }

    // Set the interface up if needed
    etiLog.printHeader(TcpLog::DBG, "Get flags.\n");
    if (ioctl(this->socket_, SIOCGIFFLAGS, &ifr) == -1) {
        etiLog.printHeader(TcpLog::ERR, "Can't get raw device flags!\n");
        perror(filename);
        return -1;
    } else {
        if (!(ifr.ifr_flags & IFF_UP)) {
            ifr.ifr_flags |= IFF_UP;
            etiLog.printHeader(TcpLog::DBG, "Set flags.\n");
            if (ioctl(this->socket_, SIOCSIFFLAGS, &ifr) == -1) {
                etiLog.printHeader(TcpLog::ERR, "Can't turn up raw device!\n");
                perror(filename);
                return -1;
            }
        }
    }

    close(this->socket_);

    ////////////////////
    // Opening device //
    ////////////////////

    if ((this->socket_ = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_CUST))) == -1) {
        etiLog.printHeader(TcpLog::ERR, "Are you logged as root?\n");
        perror(filename);
        return -1;
    }

    // ioctl to read the interface number
    etiLog.printHeader(TcpLog::DBG, "Get index.\n");
    memset(&ifr, 0, sizeof(struct ifreq));
    strncpy(ifr.ifr_name, filename, sizeof(ifr.ifr_name));
    if (ioctl(this->socket_, SIOCGIFINDEX, (char *) &ifr) == -1) {
        perror(filename);
        return -1;
    }

    // Bind to the interface name
    etiLog.printHeader(TcpLog::DBG, "Bind interface.\n");
    memset(&saddr, 0, sizeof(struct sockaddr_ll));
    saddr.sll_family = AF_PACKET;
    saddr.sll_protocol = ARPHRD_RAWHDLC;
    saddr.sll_ifindex = ifr.ifr_ifindex;
    if (bind(this->socket_, (struct sockaddr *) &saddr, sizeof(saddr)) == -1) {
        etiLog.printHeader(TcpLog::ERR, "Can't bind raw device!\n");
        perror(filename);
        return -1;
    }
#endif

    return 0;
}


int DabOutputRaw::Write(void* buffer, int size)
{
    // Encoding data
    memcpy(this->buffer_, buffer, size);
    memset(this->buffer_ + size, 0x55, 6144 - size);
    for (int i = 0; i < 6144; ++i) {
        this->buffer_[i] = revTable[this->buffer_[i]];
    }

    // Writting data
#ifdef _WIN32
    DWORD result;
    if(!DeviceIoControl(this->socket_, IoctlCodeTxFrame, this->buffer_, 6144,
        NULL, 0, &result, NULL)) {
            goto RAW_WRITE_ERROR;
    }
#else
    /*
    if (write(this->socket_, this->buffer_ + 1, 6143) != 6143) {
        goto RAW_WRITE_ERROR;
    }
    */
    if (this->isCyclades_) {
        if (write(this->socket_, this->buffer_ + 1, 6143) != 6143) {
            goto RAW_WRITE_ERROR;
        }
    } else {
        int ret = send(this->socket_, this->buffer_, 6144, 0);
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


int DabOutputRaw::Close()
{
#ifdef _WIN32
    CancelIo(this->socket_);
    CloseHandle(this->socket_);
    return 0;
#else
    if (close(this->socket_) == 0) {
        this->socket_ = -1;
        return 0;
    }
    perror("Can't close raw socket");
#endif

    return -1;
}

