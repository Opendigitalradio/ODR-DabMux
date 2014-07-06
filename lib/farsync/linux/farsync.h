/*
 *      FarSync OEM driver for Linux
 *
 *      Copyright (C) 2001-2012 FarSite Communications Ltd.
 *      www.farsite.co.uk
 *		$Id: farsync.h 1471 2013-09-10 08:01:11Z kevinc $
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 *      Author: R.J.Dunlop      <bob.dunlop@farsite.co.uk>
 *
 *      For the most part this file only contains structures and information
 *      that is visible to applications outside the driver. Shared memory
 *      layout etc is internal to the driver and described within farsync.c.
 *      Overlap exists in that the values used for some fields within the
 *      ioctl interface extend into the cards firmware interface so values in
 *      this file may not be changed arbitrarily.
 */

#define FST_NAME                "fst"           /* In debug/info etc */
#define FST_NDEV_NAME           "sync"          /* For net interface */
#define FST_DEV_NAME            "farsync"       /* For misc interfaces */

/*      User version number
 *
 *      This version number is incremented with each official release of the
 *      package and is a simplified number for normal user reference.
 *      Individual files are tracked by the version control system and may
 *      have individual versions (or IDs) that move much faster than the
 *      the release version as individual updates are tracked.
 */
#define FST_USER_VERSION        "2.1.8"
#define FST_PATCH_LEVEL         "02"
#ifdef __x86_64__
#define FST_PLATFORM            "64bit"
#else
#define FST_PLATFORM            "32bit"
#endif
#define FST_ADDITIONAL          FST_BUILD_NO

#define FST_INCLUDES_CHAR

struct fst_device_stats
{
	unsigned long	rx_packets;	/* total packets received	*/
	unsigned long	tx_packets;	/* total packets transmitted	*/
	unsigned long	rx_bytes;	/* total bytes received 	*/
	unsigned long	tx_bytes;	/* total bytes transmitted	*/
	unsigned long	rx_errors;	/* bad packets received		*/
	unsigned long	tx_errors;	/* packet transmit problems	*/
	unsigned long	rx_dropped;	/* no space in linux buffers	*/
	unsigned long	tx_dropped;	/* no space available in linux	*/
	unsigned long	multicast;	/* multicast packets received	*/
	unsigned long	collisions;

	/* detailed rx_errors: */
	unsigned long	rx_length_errors;
	unsigned long	rx_over_errors;	/* receiver ring buff overflow	*/
	unsigned long	rx_crc_errors;	/* recved pkt with crc error	*/
	unsigned long	rx_frame_errors;/* recv'd frame alignment error */
	unsigned long	rx_fifo_errors;	/* recv'r fifo overrun		*/
        unsigned long	rx_missed_errors;	/* receiver missed packet	*/

	/* detailed tx_errors */
	unsigned long	tx_aborted_errors;
	unsigned long	tx_carrier_errors;
	unsigned long	tx_fifo_errors;
	unsigned long	tx_heartbeat_errors;
	unsigned long	tx_underrun_errors;
	
	/* for cslip etc */
	unsigned long	rx_compressed;
	unsigned long	tx_compressed;
};

#define COM_STOP_BITS_1       0
#define COM_STOP_BITS_1_5     1
#define COM_STOP_BITS_2       2

#define COM_NO_PARITY         0
#define COM_ODD_PARITY        1
#define COM_EVEN_PARITY       2
#define COM_FORCE_PARITY_1    3
#define COM_FORCE_PARITY_0    4

#define COM_FLOW_CONTROL_NONE     1
#define COM_FLOW_CONTROL_RTSCTS   2
#define COM_FLOW_CONTROL_XONXOFF  3

struct fstioc_async_conf {
  unsigned char flow_control;
  unsigned char stop_bits;
  unsigned char parity;
  unsigned char word_length;
  unsigned char xon_char;
  unsigned char xoff_char;
};

/*      Ioctl call command values
 *
 *      The first three private ioctls are used by the sync-PPP module,
 *      allowing a little room for expansion we start our numbering at 10.
 */
#define FSTWRITE        (SIOCDEVPRIVATE+4)
#define FSTCPURESET     (SIOCDEVPRIVATE+5)
#define FSTCPURELEASE   (SIOCDEVPRIVATE+6)
#define FSTGETCONF      (SIOCDEVPRIVATE+7)
#define FSTSETCONF      (SIOCDEVPRIVATE+8)
#define FSTSNOTIFY      (SIOCDEVPRIVATE+9)
#define FSTGSTATE       (SIOCDEVPRIVATE+10)
#define FSTSYSREQ       (SIOCDEVPRIVATE+11)
#define FSTGETSHELL     (SIOCDEVPRIVATE+12)
#define FSTSETMON       (SIOCDEVPRIVATE+13)
#define FSTSETPORT      (SIOCDEVPRIVATE+14)
#define FSTCMD          (SIOCDEVPRIVATE+15)

/*      FSTWRITE
 *
 *      Used to write a block of data (firmware etc) before the card is running
 */
struct fstioc_write {
        unsigned int  size;
        unsigned int  offset;
        unsigned char data[0];
};

struct fstioc_control_request {
   #define FSCONTROLREQUEST_VERSION       1

   __u32  uVersion;      // Version of this structure
   __u8   bDirection;    // 1 ==> HostToDevice, 0 ==> DeviceToHost
   __u8   byRequest;
   __u16 wValue;
   __u16 wIndex;
   __u16 wDataLength;
   __u8   Data[256]; 
 };


/*      FSTCPURESET and FSTCPURELEASE
 *
 *      These take no additional data.
 *      FSTCPURESET forces the cards CPU into a reset state and holds it there.
 *      FSTCPURELEASE releases the CPU from this reset state allowing it to run,
 *      the reset vector should be setup before this ioctl is run.
 */

/*      FSTGETCONF and FSTSETCONF
 *
 *      Get and set a card/ports configuration.
 *      In order to allow selective setting of items and for the kernel to
 *      indicate a partial status response the first field "valid" is a bitmask
 *      indicating which other fields in the structure are valid.
 *      Many of the field names in this structure match those used in the
 *      firmware shared memory configuration interface and come originally from
 *      the NT header file Smc.h
 *
 *      When used with FSTGETCONF this structure should be zeroed before use.
 *      This is to allow for possible future expansion when some of the fields
 *      might be used to indicate a different (expanded) structure.
 */
struct fstioc_info {
        unsigned int   valid;           /* Bits of structure that are valid */
        unsigned int   nports;          /* Number of serial ports */
        unsigned int   type;            /* Type index of card */
        unsigned int   state;           /* State of card */
        unsigned int   index;           /* Index of port ioctl was issued on */
        unsigned int   smcFirmwareVersion;
        unsigned long  kernelVersion;   /* What Kernel version we are working with */
        unsigned short lineInterface;   /* Physical interface type */
        unsigned char  proto;           /* Line protocol */
        unsigned char  internalClock;   /* 1 => internal clock, 0 => external */
        unsigned int   lineSpeed;       /* Speed in bps */
        unsigned int   estLineSpeed;    /* Estimated speed in bps */
        unsigned int   v24IpSts;        /* V.24 control input status */
        unsigned int   v24OpSts;        /* V.24 control output status */
        unsigned short clockStatus;     /* lsb: 0=> present, 1=> absent */
        unsigned short cableStatus;     /* lsb: 0=> present, 1=> absent */
        unsigned short cardMode;        /* lsb: LED id mode */
        unsigned short debug;           /* Debug flags */
        unsigned char  transparentMode; /* Not used always 0 */
        unsigned char  invertClock;     /* Invert clock feature for syncing */
        unsigned char  asyncAbility ;   /* The ability to do async */
        unsigned char  synthAbility;    /* The ability to syth a clock */
        unsigned char  extendedClocking;/* New T4e clock modes */
        unsigned char  startingSlot;    /* Time slot to use for start of tx */
        unsigned char  clockSource;     /* External or internal */
        unsigned char  framing;         /* E1, T1 or J1 */
        unsigned char  structure;       /* unframed, double, crc4, f4, f12, */
                                        /* f24 f72 */
        unsigned char  interface;       /* rj48c or bnc */
        unsigned char  coding;          /* hdb3 b8zs */
        unsigned char  lineBuildOut;    /* 0, -7.5, -15, -22 */
        unsigned char  equalizer;       /* short or lon haul settings */
        unsigned char  loopMode;        /* various loopbacks */
        unsigned char  range;           /* cable lengths */
        unsigned char  txBufferMode;    /* tx elastic buffer depth */
        unsigned char  rxBufferMode;    /* rx elastic buffer depth */
        unsigned char  losThreshold;    /* Attenuation on LOS signal */
        unsigned char  idleCode;        /* Value to send as idle timeslot */
        unsigned int   receiveBufferDelay; /* delay thro rx buffer timeslots */
        unsigned int   framingErrorCount; /* framing errors */
        unsigned int   codeViolationCount; /* code violations */
        unsigned int   crcErrorCount;   /* CRC errors */
        int            lineAttenuation; /* in dB*/
        unsigned short lossOfSignal;
        unsigned short receiveRemoteAlarm;
        unsigned short alarmIndicationSignal;
        unsigned short _reserved[64];
        unsigned char  ignoreCarrier;  /* If set transmit regardless of carrier state */
        unsigned char  numTxBuffers;   /* No of tx buffers in card window */
        unsigned char  numRxBuffers;   /* No of rx buffers in card window */
        unsigned int   txBufferSize;   /* Size of tx buffers in card window */
        unsigned int   rxBufferSize;   /* Size of rx buffers in card window */
        unsigned char  terminalType;  /* Additional hdsl */
        unsigned char  annexType;
        unsigned char  encap;
        unsigned char  testMode;
        unsigned char  backoff;
        unsigned char  bLineProbingEnable;
        unsigned char  snrth;
        unsigned char  lpath;
        unsigned short vpi;
        unsigned short vci;
        unsigned char  activationStatus;
        unsigned char  noCommonModeStatus;
        unsigned char  transceiverStatus1;
        unsigned char  transceiverStatus2;
        unsigned char  lineLoss;
        char           signalQuality;
        unsigned char  nearEndBlockErrorCount;
        char           signalToNoiseRatio;
        unsigned char  erroredSecondCount;
        unsigned char  severelyErroredSecondCount;
        unsigned char  lossOfSyncWordSecondCount;
        unsigned char  unavailableSecondCount;
        char           frequencyDeviation;
        char           negotiatedPowerBackOff;
        unsigned char  negotiatedPSD;
        unsigned char  negotiatedBChannels;
        unsigned char  negotiatedZBits;
        unsigned short negotiatedSyncWord;
        unsigned char  negotiatedStuffBits;
        unsigned char  chipVersion;
        unsigned char  firmwareVersion;
        unsigned char  romVersion;
        unsigned short atmTxCellCount;
        unsigned short atmRxCellCount;
        unsigned short atmHecErrorCount;
        unsigned int   atmCellsDropped;
        unsigned char  transmitMSBFirst;
        unsigned char  receiveMSBFirst;
        unsigned char  xpldVersion;
        unsigned char  farEndCountryCode[2]; 
        unsigned char  farEndProviderCode[4];
        unsigned char  farEndVendorInfo[2];
        unsigned char  utopiaAtmStatus;
        unsigned int   termination;
        unsigned int   txRxStart;
        unsigned char  enableNRZIClocking;   /* Ver 1 addition */
        unsigned char  lowLatency;           /* Ver 1 addition */
        struct fst_device_stats stats;       /* Ver 1 addition */
        struct fstioc_async_conf async_conf; /* Ver 2 addition */
        unsigned char  iocinfo_version;      /* Ver 2 addition */
        unsigned char  extSyncClockEnable;   /* Ver 3 addition */
        unsigned char  extSyncClockOffset;   /* Ver 3 addition */
        unsigned int   extSyncClockRate;     /* Ver 3 addition */
        unsigned char  ppsEnable;            /* Ver 3 addition */  
        unsigned char  ppsOffset;            /* Ver 3 addition */
        unsigned char  cardRevMajor;         /* Ver 4 addition */
        unsigned char  cardRevMinor;         /* Ver 4 addition */
        unsigned char  cardRevBuild;         /* Ver 4 addition */
        unsigned int   features;             /* Ver 4 addition */
};

#define FST_MODE_HDLC        0
#define FST_MODE_TRANSPARENT 1
#define FST_MODE_BISYNC      2
#define FST_MODE_ASYNC       3
#define lowLatencyDisable 0
#define lowLatencyRx      1
#define lowLatencyTx      2
#define lowLatencyRxTx    3

/*
 *      FSTSNOTIFY
 *
 */
#define FST_NOTIFY_OFF        0
#define FST_NOTIFY_ON         1
#define FST_NOTIFY_EXTENDED   2
#define FST_NOTIFY_BASIC_SIZE 2*sizeof(int)

/*      FSTGSTATE
 *
 *      Used to query why a state change message has been issued by the driver
 *      It could be because there was a change in line states or that the txq
 *      has reached an empty state
 */
struct fstioc_status {
  int carrier_state;
  int txq_length;
  int rxq_length;
  struct fst_device_stats stats;
};

/*      FSTSYSREQ
 *
 *      Used to provide a simple transparent command/repsonse interface between
 *      an application and the firmware running on the card
 */
struct fstioc_req {
  unsigned short msg_type;
  unsigned short msg_len;
  unsigned short ret_code;
  unsigned short i_reg_idx;
  unsigned short value;
  unsigned char u_msg[16];
  unsigned char u_msg_reserved[16];
  unsigned char u_reserved[4];
};

#define MSG_FIFO_DEF_SLAVE_1X  0x0001
#define MSG_FIFO_DEF_SLAVE_16X 0x0002
#define MSG_FIFO_DEF_MASTER    0x0003
#define MSG_FIFO_EEPROM_RD     0x769b
#define MSG_FIFO_EEPROM_WR     0xcd4a
#define RSP_FIFO_SUCCESS       0x0000
#define RSP_FIFO_FAILURE       0x0001


/*      FSTSETMON
 *
 *      Used to provide a simple monitoring data 
 */
#define FSTIOC_MON_VERSION 0
#define FST_MON_RX         0
#define FST_MON_TX         1

struct fstioc_mon {
  unsigned char version;
  unsigned char tx_rx_ind;
  unsigned int  sequence;
  unsigned long timestamp;
  unsigned int  length;
};

/*      FSTSETPORT
 *
 *      Used to provide a DSL port control
 */
#define FST_DSL_PORT_NORMAL         0
#define FST_DSL_PORT_ACTIVE         1

/*      FSTCMD
 *
 *      Used to read and write card data
 */
#define FSTCMD_GET_SERIAL             0
#define FSTCMD_SET_V24O               1
#define FSTCMD_GET_VERSION            2
#define FSTCMD_SET_VERSION            3
#define FSTCMD_GET_INTS               4
#define FSTCMD_RESET_INTS             5
#define FSTCMD_RESET_STATS            6
#define FSTCMD_SET_READV              7
#define FSTCMD_SET_CHAR               8
#define FSTCMD_GET_PRESERVE_SIGNALS   9
#define FSTCMD_SET_PRESERVE_SIGNALS  10
#define FSTCMD_SET_LATENCY           11
#define FSTCMD_UPDATE_CLOCK          12
#define FSTCMD_SET_CUSTOM_RATE       13

#ifdef __x86_64__
#define fstioc_info_sz_old     316
#define fstioc_info_sz_ver1    504
#define fstioc_info_sz_ver2    512
#define fstioc_info_sz_ver3    528
#define fstioc_info_sz_ver4    536
#define fstioc_info_sz_current sizeof(struct fstioc_info)
#else
#define fstioc_info_sz_old     312
#define fstioc_info_sz_ver1    408
#define fstioc_info_sz_ver2    416
#define fstioc_info_sz_ver3    428
#define fstioc_info_sz_ver4    436
#define fstioc_info_sz_current sizeof(struct fstioc_info)
#endif

#define FST_VERSION         4
#define FST_VERSION_CURRENT FST_VERSION
#define FST_VERSION_V3	    3
#define FST_VERSION_V2	    2
#define FST_VERSION_V1      1
#define FST_VERSION_OLD     0

#define FST_READV_NORMAL 0
#define FST_READV_SYNC   1
#define FST_READV_SYNC2  2

struct fstioc_char_data {
  unsigned char queue_len;
  unsigned char threshold;
  unsigned char pad[14];
};

struct fstioc_latency_data {
  unsigned int tx_size;
  unsigned int rx_size;
  unsigned int rate;
};

struct fstioc_cmd {
  unsigned int version;
  unsigned int command;
  unsigned int status;
  unsigned int input_data_len;
  unsigned int output_data_len;
  unsigned char * data_ptr;
};

#define FST_CUSTOM_RATE_CONFIG_VERSION 1
#define FST_CUSTOM_RATE_CONFIG_LENGTH (33+1)
#define FST_CUSTOM_RATE_CLOCK_SLAVE 0
#define FST_CUSTOM_RATE_CLOCK_LOW_SLAVE 1
#define FST_CUSTOM_RATE_CLOCK_LOW_MASTER 2
#define FST_CUSTOM_CLOCK_MULTIPLIER_1 1
#define FST_CUSTOM_CLOCK_MULTIPLIER_16 2

struct fstioc_custom_rate_config {
  unsigned int version;
  unsigned int rate;
  unsigned int permanent;
  unsigned int multiplier;  /* 1 or 16 */
  unsigned int clock_type;  /* slave, low_slave, low_master */
  char rate_info[FST_CUSTOM_RATE_CONFIG_LENGTH];
};

/* "valid" bitmask */
#define FSTVAL_NONE     0x00000000      /* Nothing valid (firmware not running).
                                         * Slight misnomer. In fact nports,
                                         * type, state and index will be set
                                         * based on hardware detected.
                                         */
#define FSTVAL_OMODEM   0x0000001F      /* First 5 bits correspond to the
                                         * output status bits defined for
                                         * v24OpSts
                                         */
#define FSTVAL_SPEED    0x00000020      /* internalClock, lineSpeed, clockStatus
                                         */
#define FSTVAL_CABLE    0x00000040      /* lineInterface, cableStatus */
#define FSTVAL_IMODEM   0x00000080      /* v24IpSts */
#define FSTVAL_CARD     0x00000100      /* nports, type, state, index,
                                         * smcFirmwareVersion
                                         */
#define FSTVAL_PROTO    0x00000200      /* proto */
#define FSTVAL_MODE     0x00000400      /* cardMode */
#define FSTVAL_PHASE    0x00000800      /* Clock phase */
#define FSTVAL_TE1      0x00001000      /* T1E1 Configuration */
#define FSTVAL_BUFFERS  0x00002000      /* Tx and Rx buffer settings */
#define FSTVAL_DSL_S1   0x00004000      /* DSL-S1 Configuration */
#define FSTVAL_T4E      0x00008000      /* T4E Mk II Configuration */
#define FSTVAL_FLEX     0x00010000      /* FarSync Flex */
#define FSTVAL_ASYNC    0x00020000      /* Async config */
#define FSTVAL_DEBUG    0x80000000      /* debug */
#define FSTVAL_ALL      0x000FFFFF      /* Note: does not include DEBUG flag */

/* "type" */
#define FST_TYPE_NONE     0             /* Probably should never happen */
#define FST_TYPE_T2P      1             /* T2P X21 2 port card */
#define FST_TYPE_T4P      2             /* T4P X21 4 port card */
#define FST_TYPE_T1U      3             /* T1U X21 1 port card */
#define FST_TYPE_T2U      4             /* T2U X21 2 port card */
#define FST_TYPE_T4U      5             /* T4U X21 4 port card */
#define FST_TYPE_TE1      6             /* T1E1 X21 1 port card */
#define FST_TYPE_DSL_S1   7             /* DSL-S1 card */
#define FST_TYPE_T4E      8             /* T4E Mk II */
#define FST_TYPE_FLEX1    9             /* FarSync Flex 1 port */
#define FST_TYPE_T4UE    10             /* T4UE 4 port PCI Express */
#define FST_TYPE_T2UE    11             /* T2UE 2 port PCI Express */
#define FST_TYPE_T4Ep    12             /* T4E+ 4 port card */
#define FST_TYPE_T2U_PMC 13             /* T2U_PMC 2 port PCI card */
#define FST_TYPE_TE1e    14             /* TE1e X21 1 port PCI Express */
#define FST_TYPE_T2Ee    15             /* T2Ee 2 port PCI Express */
#define FST_TYPE_T4Ee    16             /* T4Ee 4 port PCI Express */
#define FST_TYPE_FLEX2   17             /* FarSync Flex 1 port (v2) */

/* "family" */
#define FST_FAMILY_TXP  0               /* T2P or T4P */
#define FST_FAMILY_TXU  1               /* T1U or T2U or T4U */

/* "state" */
#define FST_UNINIT      0               /* Raw uninitialised state following
                                         * system startup */
#define FST_RESET       1               /* Processor held in reset state */
#define FST_DOWNLOAD    2               /* Card being downloaded */
#define FST_STARTING    3               /* Released following download */
#define FST_RUNNING     4               /* Processor running */
#define FST_BADVERSION  5               /* Bad shared memory version detected */
#define FST_HALTED      6               /* Processor flagged a halt */
#define FST_IFAILED     7               /* Firmware issued initialisation failed
                                         * interrupt
                                         */
/* "lineInterface" */
#define V24             1
#define X21             2
#define V35             3
#define X21D            4
#define NOCABLE         5
#define RS530_449       6
#define T1              7
#define E1              8
#define J1              9
#define SHDSL           10
#define RS485           11
#define UX35C           12
#define RS485_FDX       13

#ifndef IF_IFACE_SHDSL
#define IF_IFACE_SHDSL      0x1007      /* SHDSL (FarSite)              */
#define IF_IFACE_RS530_449  0x1008      /* RS530_449 (FarSite)          */
#define IF_IFACE_RS485      0x1009      /* RS485     (FarSite)          */
#endif
#define IF_IFACE_RS485_FDX  0x100A      /* RS485 Full Duplex (Farsite)  */
#define IF_IFACE_UX35C      0x100B      /* UX35C (Farsite)              */

/* "proto" */
#define FST_HDLC        1               /* Cisco compatible HDLC */
#define FST_PPP         2               /* Sync PPP */
#define FST_MONITOR     3               /* Monitor only (raw packet reception) */
#define FST_RAW         4               /* Two way raw packets */
#define FST_GEN_HDLC    5               /* Using "Generic HDLC" module */

/* "internalClock" */
#define INTCLK          1
#define EXTCLK          0

/*
 *  The bit pattern for extendedClocking is
 *  8   4   2   1  |8    4    2    1
 *  ec             |ttrx tttx irx  itx
 */

#define	EXT_CLOCK_NONE    0x00
#define EXT_CLOCK_ERX_ETX 0x80
#define EXT_CLOCK_ERX_ITX 0x81
#define EXT_CLOCK_IRX_ETX 0x82
#define EXT_CLOCK_IRX_ITX 0x83
#define EXT_CLOCK_DTE_TT  0x84
#define EXT_CLOCK_DCE_TT  0x8B

/* "v24IpSts" bitmask */
#define IPSTS_CTS       0x00000001      /* Clear To Send (Indicate for X.21) */
#define IPSTS_INDICATE  IPSTS_CTS
#define IPSTS_DSR       0x00000002      /* Data Set Ready (T2P Port A) */
#define IPSTS_DCD       0x00000004      /* Data Carrier Detect */
#define IPSTS_RI        0x00000008      /* Ring Indicator (T2P Port A) */
#define IPSTS_TMI       0x00000010      /* Test Mode Indicator (Not Supported)*/

/* "v24OpSts" bitmask */
#define OPSTS_RTS       0x00000001      /* Request To Send (Control for X.21) */
#define OPSTS_CONTROL   OPSTS_RTS
#define OPSTS_DTR       0x00000002      /* Data Terminal Ready */
#define OPSTS_DSRS      0x00000004      /* Data Signalling Rate Select (Not
                                         * Supported) */
#define OPSTS_SS        0x00000008      /* Select Standby (Not Supported) */
#define OPSTS_LL        0x00000010      /* Local Loop */
#define OPSTS_DCD       0x00000020      /* Only when DCD is enabled as an output */
#define OPSTS_RL        0x00000040      /* Remote Loop */

/* "cardMode" bitmask */
#define CARD_MODE_IDENTIFY      0x0001

/*
 * TxRx Start Parameters
 */
#define START_TX 1
#define START_RX 2
#define START_TX_AND_RX (START_TX | START_RX)
#define START_DEFAULT START_TX_AND_RX

/* 
 * Constants for T1/E1 configuration
 */

/*
 * Clock source
 */
#define CLOCKING_SLAVE       0
#define CLOCKING_MASTER      1

/*
 * Framing
 */
#define FRAMING_E1           0
#define FRAMING_J1           1
#define FRAMING_T1           2

/*
 * Structure
 */
#define STRUCTURE_UNFRAMED   0
#define STRUCTURE_E1_DOUBLE  1
#define STRUCTURE_E1_CRC4    2
#define STRUCTURE_E1_CRC4M   3
#define STRUCTURE_T1_4       4
#define STRUCTURE_T1_12      5
#define STRUCTURE_T1_24      6
#define STRUCTURE_T1_72      7

/*
 * Interface
 */
#define INTERFACE_RJ48C      0
#define INTERFACE_BNC        1

/*
 * Coding
 */

#define CODING_HDB3               0
#define CODING_NRZ                1
#define CODING_CMI                2
#define CODING_CMI_HDB3           3
#define CODING_CMI_B8ZS           4
#define CODING_AMI                5
#define CODING_AMI_ZCS            6
#define CODING_B8ZS               7
#define CODING_NRZI               8
#define CODING_FM0                9
#define CODING_FM1                10
#define CODING_MANCHESTER         11
#define CODING_DIFF_MANCHESTER    12

/*
 * Line Build Out
 */
#define LBO_0dB              0
#define LBO_7dB5             1
#define LBO_15dB             2
#define LBO_22dB5            3

/*
 * Range for long haul t1 > 655ft
 */
#define RANGE_0_133_FT       0
#define RANGE_0_40_M         RANGE_0_133_FT
#define RANGE_133_266_FT     1
#define RANGE_40_81_M        RANGE_133_266_FT
#define RANGE_266_399_FT     2
#define RANGE_81_122_M       RANGE_266_399_FT
#define RANGE_399_533_FT     3
#define RANGE_122_162_M       RANGE_399_533_FT
#define RANGE_533_655_FT     4
#define RANGE_162_200_M      RANGE_533_655_FT
/*
 * Receive Equaliser
 */
#define EQUALIZER_SHORT      0
#define EQUALIZER_LONG       1

/*
 * Loop modes
 */
#define LOOP_NONE            0
#define LOOP_LOCAL           1
#define LOOP_PAYLOAD_EXC_TS0 2
#define LOOP_PAYLOAD_INC_TS0 3
#define LOOP_REMOTE          4

/*
 * Buffer modes
 */
#define BUFFER_2_FRAME       0
#define BUFFER_1_FRAME       1
#define BUFFER_96_BIT        2
#define BUFFER_NONE          3

/*
 * DSL Equipment types
 */
#define EQUIP_TYPE_REMOTE    0
#define EQUIP_TYPE_CENTRAL   1

/*
 * DSL Operating modes
 */
#define ANNEX_A              1    /* US */
#define ANNEX_B              0    /* EU */

/*
 * DSL ATM Encapsulation methods
 */
#define ENCAP_PPP            0
#define ENCAP_MPOA           1
#define MPOA_HEADER_LEN      8

/*
 * DSL Test Modes
 */
#define TEST_MODE_NONE                        0
#define TEST_MODE_DEFAULT TEST_MODE_NONE

#define TEST_MODE_ALTERNATING_SINGLE_PULSE    1
#define TEST_MODE_ANALOG_TRANSPARENT_LOOP     4
#define TEST_MODE_ANALOG_NON_TRANSPARENT_LOOP 8
#define TEST_MODE_TRANSMIT_SC_SR              9
#define TEST_MODE_TRANSMIT_TC_PAM_SCRONE      10
#define TEST_MODE_LINE_DRIVER_NO_SIGNAL       11
#define TEST_MODE_AGC_TO_LINE_DRIVER_LOOP     12

#define TEST_MODE_LOOP_TDM_TO_LINE            16
#define TEST_MODE_LOOP_PAYLOAD_TO_LINE        17

/*      Debug support
 *
 *      These should only be enabled for development kernels, production code
 *      should define FST_DEBUG=0 in order to exclude the code.
 *      Setting FST_DEBUG=1 will include all the debug code but in a disabled
 *      state, use the FSTSETCONF ioctl to enable specific debug actions, or
 *      FST_DEBUG can be set to prime the debug selection.
 */
#define FST_DEBUG       0x0000
#if FST_DEBUG

extern int fst_debug_mask;              /* Bit mask of actions to debug, bits
                                         * listed below. Note: Bit 0 is used
                                         * to trigger the inclusion of this
                                         * code, without enabling any actions.
                                         */
#define DBG_INIT        0x0002          /* Card detection and initialisation */
#define DBG_OPEN        0x0004          /* Open and close sequences */
#define DBG_PCI         0x0008          /* PCI config operations */
#define DBG_IOCTL       0x0010          /* Ioctls and other config */
#define DBG_INTR        0x0020          /* Interrupt routines (be careful) */
#define DBG_TX          0x0040          /* Packet transmission */
#define DBG_RX          0x0080          /* Packet reception */
#define DBG_CMD         0x0100          /* Port command issuing */
#define DBG_ATM         0x0200          /* ATM processing */
#define DBG_TTY         0x0400          /* PPPd processing */
#define DBG_USB         0x0800          /* USB device */
#define DBG_ASY         0x1000          /* Async functions */
#define DBG_FIFO        0x2000          /* Fifo functions */
#define DBG_ASS         0x0001          /* Assert like statements. Code that
                                         * should never be reached, if you see
                                         * one of these then I've been an ass
                                         */
#endif  /* FST_DEBUG */

