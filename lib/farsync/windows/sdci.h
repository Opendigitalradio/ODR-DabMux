/*******************************************************************************
*                                                                 
* Program     : FarSync (generic)
*                                                                 
* File        : sdci.h
*                                                                 
* Description : This header file is based on the Equates and Structure Layouts 
*               page of the SDCI section of the July '99 MSDN
*                                                    
* Modifications
*
* Version 2.1.0  01Mar01  WEB  Add QuickStart and GetLinkInterface
* Version 2.2.0  18Jul01  WEB  Certification candidate
* Version 2.2.1  07Aug01  JPG  Add FarSyncReadSignals
* Version 2.2.2  03Sep01  WEB  Extend USERDPC definition to include indication
*                              param instead of bReadIR
*                22Oct01  MJD  Added Transparent Mode support - defined 
*                              LinkOption_Transparent
* Version 3.0.0  12Sep02  WEB  Add dma mode and cardinfoex support definitions
* Version 3.1.0  07Nov02  WEB  Add interrupt handshake mode support definitions
* Version 3.2.0  09Dec02  WEB  Add LinkOption_InvertRxClock
*                              Add ResetStats IOCTL and extended InterfaceRecord
*                              Complete CardInfoEx definition
* Version 3.3.0  30Apr04  WEB  Extend CardInfoEx to include physical + IO address
* Version 3.3.1  03Mar05  WEB  Removed SA_TxAbort which was previously incorrectly
*                              named 
* Version 4.0.1  03Mar05  WEB  Correct IOCTL_SDCI_xxx definitions. Now requires
*                              CTL_CODE to be defined i.e winioctl.h (user-mode) or
*                              wdm.h (kernel-mode) must be included before this file.
*                              Add USERDPC_INDICATION_TX, FarSyncSetUserDpcEx,
*                              IoctlCodeFarSyncSetPortConfig,
*                              IoctlCodeFarSyncGetPortConfig,
*                              IoctlCodeFarSyncSetSerialConfig,
*                              IoctlCodeFarSyncGetSerialConfig
*                              IoctlCodeFarSyncSetCTBusConfig &
*                              IoctlCodeFarSyncGetCTBusConfig
* Version 4.0.1.1 03Mar05 WEB  Update FS_XXX_CONFIG structures
* Version 4.0.1.2 04Mar05 WEB  Update error counter explanations,
*                              FarSync-specific error codes & further
*                              rationalisations
* Version 4.1.0.0 15Jun05 WEB  Add TE1 and additional T4E definitions
*                              Add monitoring IOCTLs
* Version 4.1.0.1 02Aug05 MJD  Added USERDPC_INDICATION_CLOCK_SWITCH_TO_PRIMARY
* Version 4.1.0.2 09Aug05 MJD  Update FS_CLOCKING_STATUS to include uCTAInUse so
*                              that apps can tell which CT Bus the hardware is 
*                              using in slave mode, structure version now 2. 
* Version 4.2.0   12Sep05 WEB  Add async and synth detection fields to 
*                              FSCARDINFOEX
*                              SA_RxFrameTooBig is now supported by M1P
*                              Add FS_ERROR_INVALID_LENGTH definition
*
*******************************************************************************/

#ifndef __SDCI_H_ 
#define __SDCI_H_

/*****************************************************************************/
//
// There are the IOCTL code values used to communicate with the SDCI driver.
//
/*****************************************************************************/

#define IOCTL_SDCI_SetEvent                        CTL_CODE(0, (0x410 >> 2), METHOD_BUFFERED,   FILE_ANY_ACCESS)
#define IOCTL_SDCI_SetLinkCharacteristics          CTL_CODE(0, (0x420 >> 2), METHOD_BUFFERED,   FILE_ANY_ACCESS)
#define IOCTL_SDCI_SetV24OutputStatus              CTL_CODE(0, (0x430 >> 2), METHOD_BUFFERED,   FILE_ANY_ACCESS)
#define IOCTL_SDCI_TransmitFrame                   CTL_CODE(0, (0x440 >> 2), METHOD_IN_DIRECT,  FILE_ANY_ACCESS)
#define IOCTL_SDCI_AbortTransmit                   CTL_CODE(0, (0x450 >> 2), METHOD_BUFFERED,   FILE_ANY_ACCESS)
#define IOCTL_SDCI_AbortReceiver                   CTL_CODE(0, (0x460 >> 2), METHOD_BUFFERED,   FILE_ANY_ACCESS)
#define IOCTL_SDCI_OffBoardLoad                    CTL_CODE(0, (0x470 >> 2), METHOD_BUFFERED,   FILE_ANY_ACCESS)
#define IOCTL_SDCI_GetV24Status                    CTL_CODE(0, (0x620 >> 2), METHOD_NEITHER,    FILE_ANY_ACCESS)
#define IOCTL_SDCI_ReceiveFrame                    CTL_CODE(0, (0x630 >> 2), METHOD_OUT_DIRECT, FILE_ANY_ACCESS)
#define IOCTL_SDCI_FarSyncReceiveFrame             CTL_CODE(0, (0x630 >> 2), METHOD_NEITHER,    FILE_ANY_ACCESS)
#define IOCTL_SDCI_ReadInterfaceRecord             CTL_CODE(0, (0x640 >> 2), METHOD_OUT_DIRECT, FILE_ANY_ACCESS)
#define IOCTL_SDCI_FarSyncReadInterfaceRecord      CTL_CODE(0, (0x640 >> 2), METHOD_NEITHER,    FILE_ANY_ACCESS)
#define IOCTL_SDCI_FarSyncReadInterfaceRecordEx    CTL_CODE(0, (0x650 >> 2), METHOD_NEITHER,    FILE_ANY_ACCESS)
#define IOCTL_SDCI_FarSyncSetLinkInterface         CTL_CODE(0, (0x710 >> 2), METHOD_BUFFERED,   FILE_ANY_ACCESS)
#define IOCTL_SDCI_FarSyncSetUserDpc               CTL_CODE(0, (0x720 >> 2), METHOD_BUFFERED,   FILE_ANY_ACCESS)
#define IOCTL_SDCI_FarSyncSetCardMode              CTL_CODE(0, (0x730 >> 2), METHOD_BUFFERED,   FILE_ANY_ACCESS)
#define IOCTL_SDCI_FarSyncReadCardInfo             CTL_CODE(0, (0x740 >> 2), METHOD_NEITHER,    FILE_ANY_ACCESS)
#define IOCTL_SDCI_FarSyncQuickStart               CTL_CODE(0, (0x750 >> 2), METHOD_BUFFERED,   FILE_ANY_ACCESS)
#define IOCTL_SDCI_FarSyncGetLinkInterface         CTL_CODE(0, (0x760 >> 2), METHOD_NEITHER,    FILE_ANY_ACCESS)
#define IOCTL_SDCI_FarSyncReadSignals              CTL_CODE(0, (0x770 >> 2), METHOD_NEITHER,    FILE_ANY_ACCESS)
#define IOCTL_SDCI_FarSyncSetDMAMode               CTL_CODE(0, (0x780 >> 2), METHOD_BUFFERED,   FILE_ANY_ACCESS)
#define IOCTL_SDCI_FarSyncGetDMAMode               CTL_CODE(0, (0x790 >> 2), METHOD_NEITHER,    FILE_ANY_ACCESS)
#define IOCTL_SDCI_FarSyncReadCardInfoEx           CTL_CODE(0, (0x7a0 >> 2), METHOD_NEITHER,    FILE_ANY_ACCESS)
#define IOCTL_SDCI_FarSyncSetHandShakeMode         CTL_CODE(0, (0x7b0 >> 2), METHOD_BUFFERED,   FILE_ANY_ACCESS)
#define IOCTL_SDCI_FarSyncGetHandShakeMode         CTL_CODE(0, (0x7c0 >> 2), METHOD_NEITHER,    FILE_ANY_ACCESS)
#define IOCTL_SDCI_FarSyncResetStats               CTL_CODE(0, (0x7d0 >> 2), METHOD_BUFFERED,   FILE_ANY_ACCESS)
#define IOCTL_SDCI_FarSyncReadTE1Status            CTL_CODE(0, (0x7e0 >> 2), METHOD_NEITHER,    FILE_ANY_ACCESS)
#define IOCTL_SDCI_FarSyncSetUserDpcEx             CTL_CODE(0, (0x7f0 >> 2), METHOD_BUFFERED,   FILE_ANY_ACCESS)
#define IOCTL_SDCI_FarSyncSetPortConfig            CTL_CODE(0, (0x810 >> 2), METHOD_BUFFERED,   FILE_ANY_ACCESS)
#define IOCTL_SDCI_FarSyncGetPortConfig            CTL_CODE(0, (0x820 >> 2), METHOD_NEITHER,    FILE_ANY_ACCESS)
#define IOCTL_SDCI_FarSyncSetSerialConfig          CTL_CODE(0, (0x830 >> 2), METHOD_BUFFERED,   FILE_ANY_ACCESS)
#define IOCTL_SDCI_FarSyncGetSerialConfig          CTL_CODE(0, (0x840 >> 2), METHOD_NEITHER,    FILE_ANY_ACCESS)
#define IOCTL_SDCI_FarSyncSetCTBusConfig           CTL_CODE(0, (0x850 >> 2), METHOD_BUFFERED,   FILE_ANY_ACCESS)
#define IOCTL_SDCI_FarSyncGetCTBusConfig           CTL_CODE(0, (0x860 >> 2), METHOD_NEITHER,    FILE_ANY_ACCESS)
#define IOCTL_SDCI_FarSyncSetTE1Config             CTL_CODE(0, (0x870 >> 2), METHOD_BUFFERED,   FILE_ANY_ACCESS)
#define IOCTL_SDCI_FarSyncGetTE1Config             CTL_CODE(0, (0x880 >> 2), METHOD_NEITHER,    FILE_ANY_ACCESS)
#define IOCTL_SDCI_FarSyncSetCTBusBackupConfig     CTL_CODE(0, (0x890 >> 2), METHOD_BUFFERED,   FILE_ANY_ACCESS)
#define IOCTL_SDCI_FarSyncGetCTBusBackupConfig     CTL_CODE(0, (0x8a0 >> 2), METHOD_NEITHER,    FILE_ANY_ACCESS)
#define IOCTL_SDCI_FarSyncGetClockingStatus        CTL_CODE(0, (0x8b0 >> 2), METHOD_NEITHER,    FILE_ANY_ACCESS)
#define IOCTL_SDCI_FarSyncSetMonitoring            CTL_CODE(0, (0x8c0 >> 2), METHOD_BUFFERED,   FILE_ANY_ACCESS)
#define IOCTL_SDCI_FarSyncGetMonitoringStatus      CTL_CODE(0, (0x8d0 >> 2), METHOD_NEITHER,    FILE_ANY_ACCESS)

#define IoctlCodeSetEvent                       IOCTL_SDCI_SetEvent
#define IoctlCodeSetLinkChar                    IOCTL_SDCI_SetLinkCharacteristics
#define IoctlCodeSetV24                         IOCTL_SDCI_SetV24OutputStatus
#define IoctlCodeTxFrame                        IOCTL_SDCI_TransmitFrame   
#define IoctlCodeAbortTransmit                  IOCTL_SDCI_AbortTransmit
#define IoctlCodeAbortReceiver                  IOCTL_SDCI_AbortReceiver
#define IoctlCodeOffBoardLoad                   IOCTL_SDCI_OffBoardLoad
#define IoctlCodeGetV24                         IOCTL_SDCI_GetV24Status
#define IoctlCodeRxFrame                        IOCTL_SDCI_ReceiveFrame   
#define IoctlCodeFarSyncRxFrame                 IOCTL_SDCI_FarSyncReceiveFrame
#define IoctlCodeReadInterfaceRecord            IOCTL_SDCI_ReadInterfaceRecord
#define IoctlCodeFarSyncReadInterfaceRecord     IOCTL_SDCI_FarSyncReadInterfaceRecord
#define IoctlCodeFarSyncReadInterfaceRecordEx   IOCTL_SDCI_FarSyncReadInterfaceRecordEx
#define IoctlCodeFarSyncSetLinkInterface        IOCTL_SDCI_FarSyncSetLinkInterface
#define IoctlCodeSetLinkInterface               IoctlCodeFarSyncSetLinkInterface
#define IoctlCodeFarSyncSetUserDpc              IOCTL_SDCI_FarSyncSetUserDpc
#define IoctlCodeFarSyncSetCardMode             IOCTL_SDCI_FarSyncSetCardMode
#define IoctlCodeFarSyncReadCardInfo            IOCTL_SDCI_FarSyncReadCardInfo
#define IoctlCodeFarSyncQuickStart              IOCTL_SDCI_FarSyncQuickStart
#define IoctlCodeFarSyncGetLinkInterface        IOCTL_SDCI_FarSyncGetLinkInterface
#define IoctlCodeFarSyncReadSignals             IOCTL_SDCI_FarSyncReadSignals
#define IoctlCodeFarSyncSetDMAMode              IOCTL_SDCI_FarSyncSetDMAMode
#define IoctlCodeFarSyncGetDMAMode              IOCTL_SDCI_FarSyncGetDMAMode
#define IoctlCodeFarSyncReadCardInfoEx          IOCTL_SDCI_FarSyncReadCardInfoEx
#define IoctlCodeFarSyncSetHandShakeMode        IOCTL_SDCI_FarSyncSetHandShakeMode
#define IoctlCodeFarSyncGetHandShakeMode        IOCTL_SDCI_FarSyncGetHandShakeMode
#define IoctlCodeFarSyncResetStats              IOCTL_SDCI_FarSyncResetStats
#define IoctlCodeFarSyncReadTE1Status           IOCTL_SDCI_FarSyncReadTE1Status
#define IoctlCodeFarSyncSetUserDpcEx            IOCTL_SDCI_FarSyncSetUserDpcEx
#define IoctlCodeFarSyncSetPortConfig           IOCTL_SDCI_FarSyncSetPortConfig
#define IoctlCodeFarSyncGetPortConfig           IOCTL_SDCI_FarSyncGetPortConfig
#define IoctlCodeFarSyncSetSerialConfig         IOCTL_SDCI_FarSyncSetSerialConfig
#define IoctlCodeFarSyncGetSerialConfig         IOCTL_SDCI_FarSyncGetSerialConfig
#define IoctlCodeFarSyncSetCTBusConfig          IOCTL_SDCI_FarSyncSetCTBusConfig
#define IoctlCodeFarSyncGetCTBusConfig          IOCTL_SDCI_FarSyncGetCTBusConfig
#define IoctlCodeFarSyncSetTE1Config            IOCTL_SDCI_FarSyncSetTE1Config
#define IoctlCodeFarSyncGetTE1Config            IOCTL_SDCI_FarSyncGetTE1Config
#define IoctlCodeFarSyncSetCTBusBackupConfig    IOCTL_SDCI_FarSyncSetCTBusBackupConfig
#define IoctlCodeFarSyncGetCTBusBackupConfig    IOCTL_SDCI_FarSyncGetCTBusBackupConfig
#define IoctlCodeFarSyncGetClockingStatus       IOCTL_SDCI_FarSyncGetClockingStatus
#define IoctlCodeFarSyncSetMonitoring           IOCTL_SDCI_FarSyncSetMonitoring
#define IoctlCodeFarSyncGetMonitoringStatus     IOCTL_SDCI_FarSyncGetMonitoringStatus

/*****************************************************************************/
/* Constants for the driver-specific IOCtl return codes.                     */
/*****************************************************************************/
#define CEDNODMA 0xff80     /* Warning (NO DMA!) from set link chrctrstcs    */
/*****************************************************************************/
/* Equates for the link options byte 1.                                      */
/*****************************************************************************/
#define CEL4WIRE 0x80
#define CELNRZI  0x40
#define CELPDPLX 0x20
#define CELSDPLX 0x10
#define CELCLOCK 0x08
#define CELDSRS  0x04
#define CELSTNBY 0x02
#define CELDMA   0x01

/*****************************************************************************/
/* Equates for the driver set link characteristics byte 1.                   */
/*****************************************************************************/
#define CED4WIRE 0x80
#define CEDNRZI  0x40
#define CEDHDLC  0x20
#define CEDFDPLX 0x10
#define CEDCLOCK 0x08
#define CEDDMA   0x04
#define CEDRSTAT 0x02
#define CEDCSTAT 0x01

/* Nicer names for NT-style code */

#define LinkOption_4Wire           CED4WIRE
#define LinkOption_InvertRxClock   CEDDMA
#define LinkOption_NRZI            CEDNRZI
#define LinkOption_HDLC            CEDHDLC
#define LinkOption_FullDuplex      CEDFDPLX
#define LinkOption_InternalClock   CEDCLOCK
#define LinkOption_DMA             CEDDMA
#define LinkOption_ResetStatistics CEDRSTAT
#define LinkOption_Transparent     CEDCSTAT

/*****************************************************************************/
/* Equates for the output V24 interface flags.                               */
/*****************************************************************************/
#define CED24RTS 0x01
#define CED24DTR 0x02
// #define CED24DRS 0x04- not used. 0x04 is instead used by DCD 
// i.e. when configured as an output - see below
#define CED24SLS 0x08
#define CED24TST 0x10

/* Nicer names for NT-style code */

#define IR_OV24RTS  CED24RTS
#define IR_OV24DTR  CED24DTR
//#define IR_OV24DSRS CED24DRS
#define IR_OV24SlSt CED24SLS
#define IR_OV24Test CED24TST


/*****************************************************************************/
/* Equates for the input V24 interface flags.                                */
/*****************************************************************************/
#define CED24CTS 0x01
#define CED24DSR 0x02
#define CED24DCD 0x04
#define CED24RI  0x08

// CEDCR indicates the presense of a generic carrier i.e. independent of port type
// e.g. DCD for V.25, CTS for X.21
#define CEDCR  0x10

 // Note: DCD can only be used as an output signal on a FarSync T4E
 // and only then if a FarSyncSetSerialConfig has been previously
 // issued for this port, with FsSerialConfig.uDCDOutput set to
 // TRUE

/* Nicer names for NT-style code */

#define IR_IV24CTS  CED24CTS
#define IR_IV24DSR  CED24DSR
#define IR_IV24DCD  CED24DCD
#define IR_IV24RI   CED24RI
#define IR_IV24Test 0x10


/*****************************************************************************/
/* Structure for the device driver interface record.                         */
/*****************************************************************************/

#define CEDSTCRC  0         /* Frames received with incorrect CRC            */
#define CEDSTOFL  1         /* Frames received longer than the maximum       */
#define CEDSTUFL  2         /* Frames received less than 4 octets long       */
#define CEDSTSPR  3         /* Frames received ending on a non-octet bndry   */
#define CEDSTABT  4         /* Aborted frames received                       */
#define CEDSTTXU  5         /* Transmitter interrupt underruns               */
#define CEDSTRXO  6         /* Receiver    interrupt overruns                */
#define CEDSTDCD  7         /* DCD (RLSD) lost during frame reception        */
#define CEDSTCTS  8         /* CTS lost while transmitting                   */
#define CEDSTDSR  9         /* DSR drops                                     */
#define CEDSTHDW 10         /* Hardware failures - adapter errors            */

#define CEDSTMAX 11

#define SA_CRC_Error       CEDSTCRC
#define SA_RxFrameTooBig   CEDSTOFL
#define SA_RxFrameTooShort CEDSTUFL
#define SA_Spare           CEDSTSPR
#define SA_RxAbort         CEDSTABT
#define SA_TxUnderrun      CEDSTTXU
#define SA_RxOverrun       CEDSTRXO
#define SA_DCDDrop         CEDSTDCD
#define SA_CTSDrop         CEDSTCTS
#define SA_DSRDrop         CEDSTDSR
#define SA_HardwareError   CEDSTHDW

// FarSync-specific counters mapped to existing SDCI definitions 
#define SA_FramingError       SA_Spare
#define SA_RxError            SA_HardwareError
#define SA_BufferUnavailable  SA_DCDDrop
#define SA_Parity             SA_CRC_Error

#define SA_Max_Stat        CEDSTMAX

// ****************************************************************************************************
// FarSync supports the following counters. All other counter indices are not used by FarSync.

// SA_CRC_Error       Sync & Async modes
// SA_FramingError    Sync & Async modes
// SA_RxOverrun       Sync & Async modes

// SA_RxAbort         (Async) & (M1P Sync) modes only
// SA_RxError         Sync mode only
// SA_TxUnderrun      Sync mode only
// SA_RxFrameTooShort M1P Sync mode only

// SA_RxFrameTooBig   Async mode (fifo overflow) & M1P Sync

//    Note that 
//      1) Async mode is currently only supported on the T4U - it is an optional extra feature
//      2) On a TxU an SA_RxError indicates a received abort OR a rx frame length error (too big OR too small)
//      3) On an M1P an SA_RxError indicates a alternative type of RxO as reported via SA_RxOverrun
// ****************************************************************************************************

/*****************************************************************************/
/* InterfaceRecord definition                                                */
/*                                                                           */
/* For use with:                                                             */
/*                                                                           */
/*   IoctlCodeReadInterfaceRecord,                                           */
/*   IoctlCodeFarSyncReadInterfaceRecord                                     */
/*                                                                           */
/*****************************************************************************/
typedef struct _INTERFACE_RECORD
{
  int    RxFrameCount;   /* incremented after each frame rx'd */
  int    TxMaxFrSizeNow; /* max available frame size av. now  */
                         /* (changes after each Tx DevIoctl   */
                         /* to DD or after Tx completed)      */
  int    StatusCount;    /* How many status events have been  */
                         /* triggered.                        */
  UCHAR  V24In;          /* Last 'getv24 i/p' value got       */
  UCHAR  V24Out;         /* Last 'setv24 o/p' value set       */

/* The values for the indexes into the link statistics array of the */
/* various types of statistic. */

  int    StatusArray[SA_Max_Stat];

} IR, * PIR;

/*****************************************************************************/
/* InterfaceRecordEx definition                                              */
/*                                                                           */
/* For use with:                                                             */
/*                                                                           */
/*   IoctlCodeFarSyncReadInterfaceRecordEx                                   */
/*                                                                           */
/*****************************************************************************/
typedef struct _INTERFACE_RECORD_EX
{
  IR     InterfaceRecord;
  int    StatusCount;    /* How many status events have been  */
  ULONG  OpenedCount;
  ULONG  TxRequestCount;
  ULONG  TxCompleteCount;
  ULONG  RxPostedCount;
  ULONG  RxCompleteCount;
} IREX, * PIREX;

/*****************************************************************************/
/* Set link characteristics parameter block definition                       */
/*                                                                           */
/* For use with:                                                             */
/*                                                                           */
/*   IoctlCodeSetLinkChar                                                    */
/*                                                                           */
/*****************************************************************************/
typedef struct  _SLPARMS
{
  int     SLFrameSize;            /* max frame size on link, should    */
                                  /* include 2-byte CRC - max is 8K    */
  LONG    SLDataRate;             /* not used by us - external clocks  */
  UCHAR   SLOurAddress1;          /* ) e.g C1/FF or 00/00 or 01/03     */
  UCHAR   SLOurAddress2;          /* )                                 */
  UCHAR   SLLinkOptionsByte;      /* see documentation & LinkOption_*  */
  UCHAR   SLSpare1;
} SLPARMS, *PSLPARMS;

/*****************************************************************************/
/* Set link interface parameter block definition                             */
/*                                                                           */
/* For use with:                                                             */
/*                                                                           */
/*   IoctlCodeFarSyncSetLinkInterface,                                       */
/*   IoctlCodeFarSyncGetLinkInterface                                        */
/*                                                                           */
/*****************************************************************************/
#pragma pack(push, 4)     /* Note: sizeof(FsLinkIfParms) is expected to be 0x10 */
typedef struct  _FSLINKIFPARMS
{
    HANDLE Context;       // context for completion routine - not used 
    USHORT MaxFrameSize;  // maximum frame size - not used
    USHORT Interface;     // line interface: 
    ULONG  BaudRate;      // baud rate
    UCHAR  Reserved;      // reserved
} FSLINKIFPARMS, * PFSLINKIFPARMS;
#pragma pack(pop)

// USER DPC DEFINITIONS - KERNEL-MODE ONLY

#define USERDPC_INDICATION_RX     0
#define USERDPC_INDICATION_SIGNAL 1
#define USERDPC_INDICATION_ERROR  2
#define USERDPC_INDICATION_TX     3 /* indicates that the number of tx buffers 
                                       in use by the card has changed - examine
                                       *pUserDpcTxBuffersInUse for the current value */

// FarSync T4E clock notifications
#define USERDPC_INDICATION_CLOCK_RATE_CHANGED        FS_INDICATION_CLOCK_RATE_CHANGED
#define USERDPC_INDICATION_CLOCK_OUT_OF_TOLERANCE1   FS_INDICATION_CLOCK_OUT_OF_TOLERANCE1
#define USERDPC_INDICATION_CLOCK_IN_TOLERANCE1       FS_INDICATION_CLOCK_IN_TOLERANCE1
#define USERDPC_INDICATION_CLOCK_SWITCH_TO_BACKUP    FS_INDICATION_CLOCK_SWITCH_TO_BACKUP
#define USERDPC_INDICATION_CLOCK_OUT_OF_TOLERANCE2   FS_INDICATION_CLOCK_OUT_OF_TOLERANCE2
#define USERDPC_INDICATION_CLOCK_IN_TOLERANCE2       FS_INDICATION_CLOCK_IN_TOLERANCE2
#define USERDPC_INDICATION_CLOCK_SWITCH_TO_OSC       FS_INDICATION_CLOCK_SWITCH_TO_OSC
#define USERDPC_INDICATION_CLOCK_SWITCH_TO_CTA       FS_INDICATION_CLOCK_SWITCH_TO_CTA
#define USERDPC_INDICATION_CLOCK_SWITCH_TO_CTB       FS_INDICATION_CLOCK_SWITCH_TO_CTB
#define USERDPC_INDICATION_CLOCK_SWITCH_TO_PRIMARY   FS_INDICATION_CLOCK_SWITCH_TO_PRIMARY

// user DPC callback - kernel mode only
typedef VOID (*USERDPC)(PVOID /* caller's handle*/, unsigned char /* indication */);  

/*****************************************************************************/
/* Set user dpc parameter block definition                                   */
/*                                                                           */
/* For use with:                                                             */
/*                                                                           */
/*   IoctlCodeFarSyncSetUserDpc      ,                                       */
/*                                                                           */
/*****************************************************************************/
#pragma pack(push, 4)     
typedef struct  _FSUSERDPCINFO
{
  USERDPC fUserDpc;      // callback address
  PVOID   pUserDpcParam; // param to supply in callback
} FSUSERDPCINFO, * PFSUSERDPCINFO;

/*****************************************************************************/
/* Extended set user dpc parameter block definition                          */
/*                                                                           */
/* For use with:                                                             */
/*                                                                           */
/*   IoctlCodeFarSyncSetUserDpcEx    ,                                       */
/*                                                                           */
/* If used, this structure must be allocated in NonPaged memory.             */
/*                                                                           */
/*****************************************************************************/
typedef struct  _FSUSERDPCINFOEX
{
  FSUSERDPCINFO fUserDpcInfo;
  long *        pUserDpcTxBuffersInUse; // ref to a SDCI client-owned var that is used to maintain the number of tx buffers
                                        // currently in use by the card.
} FSUSERDPCINFOEX, * PFSUSERDPCINFOEX;
#pragma pack(pop)

/*****************************************************************************/
/* Card mode parameter block definition                                      */
/*                                                                           */
/* For use with:                                                             */
/*                                                                           */
/*   IoctlCodeFarSyncSetCardMode     ,                                       */
/*                                                                           */
/*****************************************************************************/
#pragma pack(push, 4)     
typedef struct  _FSCARDMODE
{
  BOOLEAN bIdentifyMode;
} FSCARDMODE, * PFSCARDMODE;
#pragma pack(pop)

/*****************************************************************************/
/* Card info parameter block definition                                      */
/*                                                                           */
/* For use with:                                                             */
/*                                                                           */
/*   IoctlCodeFarSyncReadCardInfo    ,                                       */
/*                                                                           */
/*****************************************************************************/
#pragma pack(push, 4)     
typedef struct  _FSCARDINFO
{
  #define FSCARDINFO_VERSION  1
  #define SERIAL_NO_LENGTH    8

  ULONG  uVersion;       // Version of this structure
  USHORT uDeviceId;
  USHORT uSubSystemId;
  ULONG  uNumberOfPorts;
  char   szSerialNo[SERIAL_NO_LENGTH+1];
  ULONG  uMajorRevision;
  ULONG  uMinorRevision;
  ULONG  uBuildState;
  ULONG  uCPUSpeed;
  ULONG  uMode;
} FSCARDINFO, * PFSCARDINFO;
#pragma pack(pop)

/*****************************************************************************/
/* DMA mode parameter block definition                                       */
/*                                                                           */
/* For use with:                                                             */
/*                                                                           */
/*   IoctlCodeFarSyncSetDMAMode,                                             */   
/*   IoctlCodeFarSyncGetDMAMode    ,                                         */
/*                                                                           */
/*****************************************************************************/
#pragma pack(push, 4)     
typedef struct  _FSDMAMODE
{
  #define FSDMAMODE_VERSION       1
  #define FSDMAMODE_OFF           1
  #define FSDMAMODE_ON            2
  #define FSDMAMODE_INTERMEDIATE  3 // use for processing rxs via intermediate buffer

  ULONG  uVersion;       // Version of this structure
  USHORT uTxDMAMode;
  USHORT uRxDMAMode;
} FSDMAMODE, * PFSDMAMODE;
#pragma pack(pop)

/*****************************************************************************/
/* Interrupt handshake mode parameter block definition                       */
/*                                                                           */
/* For use with:                                                             */
/*                                                                           */
/*   IoctlCodeFarSyncSetHandShakeMode,                                       */   
/*   IoctlCodeFarSyncGetHandShakeMode                                        */
/*                                                                           */
/*****************************************************************************/
#pragma pack(push, 4)     
typedef struct  _FSHANDSHAKEMODE
{
  #define FSHANDSHAKEMODE_VERSION       1
  #define FSHANDSHAKEMODE_2             2
  #define FSHANDSHAKEMODE_3             3

  ULONG  uVersion;       // Version of this structure
  USHORT uMode;
} FSHANDSHAKEMODE, * PFSHANDSHAKEMODE;
#pragma pack(pop)

/*****************************************************************************/
/* Extended card info parameter block definition                             */
/*                                                                           */
/* For use with:                                                             */
/*                                                                           */
/*   IoctlCodeFarSyncReadCardInfoEx  ,                                       */
/*                                                                           */
/*****************************************************************************/
#pragma pack(push, 4)     
typedef struct  _FSCARDINFOEX
{
  #define FSCARDINFOEX_VERSION  2

  ULONG  uVersion;           // Version of this structure

  char   szDeviceName[32+8]; // Including aligned PADDING to allow for null-terminator
  PVOID  w_memory;           // Virtual addresses of Adapter Resources
  PVOID  w_controlSpace;     
  PVOID  w_localCfg;         
  PVOID  w_ioSpace;
  ULONG  z_memory;           // Lengths of Adapter Resources
  ULONG  z_controlSpace;     
  ULONG  z_localCfg;         
  ULONG  z_ioSpace;

  ULONG  uHiVersion;  
  ULONG  uLoVersion;

  ULONG  uReservedA[30]; // these are reserved for FarSite's own use

  PVOID  p_memory;       // physical address of window - T-Series cards 
  PVOID  p_io;           // main IO address of card
  ULONG  uExtendedClockingSupported; // are (T4E) clock synths available on the card?
  ULONG  uAsyncSupported;  // does the card support async
  ULONG  uReservedB[28];   // these are currently unused

} FSCARDINFOEX, * PFSCARDINFOEX;
#pragma pack(pop)

/*****************************************************************************/
/* Port config parameter block definition                                    */
/*                                                                           */
/* For use with:                                                             */
/*                                                                           */
/*   IoctlCodeFarSyncSetPortConfig,                                          */   
/*   IoctlCodeFarSyncGetPortConfig                                           */
/*                                                                           */
/*****************************************************************************/
#pragma pack(push, 4)     
typedef struct  _FS_PORT_CONFIG
{
  #define FSPORTCONFIG_VERSION       2

  ULONG  uVersion;              // Version of this structure
  ULONG  uPortTxNumBuffers;     // 2,4,8,16,32,64,128
  ULONG  uPortRxNumBuffers;     // 2,4,8,16,32,64,128
	ULONG  uPortTxBufferLength;   // Min=2 Max=64*1024 Def= 8*1024
	ULONG  uPortRxBufferLength;   // Min=2 Max=64*1024 Def= 8*1024
  ULONG  uPortTxDMA;            // See fscfg.h for values i.e. FSDMAMODE_OFF=1, FSDMAMODE_ON=2
  ULONG  uPortRxDMA;            // See fscfg.h for values i.e. FSDMAMODE_OFF=1, FSDMAMODE_ON=2
  ULONG  uPortStartTxRx;        // See smcuser.h for values i.e. START_TX=1 | START_RX=2
  ULONG  uPortClockSource;      // See smcuser.h for values i.e. FS_CLOCK_REFERENCE_xxx
                                // Currently only supported on T4E
  ULONG  uPortTxMSB;            // FALSE ==> LSB (Default)
  ULONG  uPortRxMSB;            // FALSE ==> LSB (Default)
  ULONG  uPortMaxTxsOutstanding;// read-only - indicates how many txs can be outstanding at a time
  ULONG  uReserved[52];
} FS_PORT_CONFIG, *PFS_PORT_CONFIG;
#pragma pack(pop)

/*****************************************************************************/
/* Serial config parameter block definition                                  */
/*                                                                           */
/* For use with:                                                             */
/*                                                                           */
/*   IoctlCodeFarSyncSetSerialConfig,                                        */   
/*   IoctlCodeFarSyncGetSerialConfig                                         */
/*                                                                           */
/*****************************************************************************/
#pragma pack(push, 4)     
typedef struct  _FS_SERIAL_CONFIG
{
  #define FSSERIALCONFIG_VERSION   2

  ULONG  uVersion;              // Version of this structure
  ULONG  uPortInterface;        // See fscfg.h for values e.g. FS_LINE_INTERFACE_X21
  ULONG  uPortRate;
  ULONG  uPortClocking;         // See fscfg.h for values e.g. FS_LINE_CLOCKING_INTERNAL
	ULONG  uPortTransparentMode;  // HDLC=0 Transparent=1
  ULONG  uPortInvertRxClock;    // TRUE(1) or FALSE(0)
  ULONG  uPortEncoding;         // See fscfg for values i.e. FS_PORT_ENCODING_xxx
                                // Currently only supported on M1P

  // The following fields have been added in Version 2 of this structure and not be
  // processed if uVersion = 1
  ULONG  uExtendedClocking; // subsequent fields only used if this is TRUE
  ULONG  uInternalTxClock;  
  ULONG  uInternalRxClock;  
  ULONG  uTerminalTxClock;  
  ULONG  uTerminalRxClock;  
  ULONG  uDCDOutput;        // TRUE => DCD is an output

  ULONG  uEstimatedLineSpeed; // returned by FarSyncGetSerialConfig for T4E only

  ULONG  uReserved[54];
} FS_SERIAL_CONFIG, *PFS_SERIAL_CONFIG;
#pragma pack(pop)

/*****************************************************************************/
/* CTBus config parameter block definition                                   */
/*                                                                           */
/* For use with:                                                             */
/*                                                                           */
/*   IoctlCodeFarSyncSetCTBusConfig                                          */   
/*   IoctlCodeFarSyncGetCTBusConfig                                          */
/*                                                                           */
/*****************************************************************************/
#pragma pack(push, 4)     
typedef struct  _FS_CT_BUS_CONFIG
{
  #define FSCTBUSCONFIG_VERSION       2

  ULONG  uVersion;     // Version of this structure
  ULONG  uCTBusMode;   // See smcuser.h for values i.e. FS_CT_BUS_MODE_xxx
  ULONG  uCTBusFeed;   // See smcuser.h for values i.e. FS_CT_BUS_FEED_xxx
  ULONG  uFallback;    // TRUE=>auto switch if clock ref fails
  ULONG  uReserved[60];
} FS_CT_BUS_CONFIG, *PFS_CT_BUS_CONFIG;
#pragma pack(pop)

/*****************************************************************************/
/* Clocking Status block definition                                          */
/*                                                                           */
/* For use with:                                                             */
/*                                                                           */
/*   IoctlCodeFarSyncGetClockingStatus                                       */
/*                                                                           */
/*****************************************************************************/
#pragma pack(push, 4)     
typedef struct  _FS_CLOCKING_STATUS
{
  #define FSCLOCKINGSTATUS_VERSION       2

  ULONG  uVersion;               // Version of this structure
  ULONG  uPrimaryClockStatus;    // 0=out-of-tolerance, 1=good
  ULONG  uBackupClockStatus;     // 0=out-of-tolerance, 1=good
  ULONG  uCurrentConfigReference; // See smcuser.h for values i.e. FS_CT_CONFIG_xxx
  ULONG  uCTAStatus;             // 0=out-of-tolerance, 1=good
  ULONG  uCTBStatus;             // 0=out-of-tolerance, 1=good
  USHORT uCurrentStatusSummary;  // bit map status word describing clocking state/config
                                 // see smcuser.h for bit definitions
  USHORT uReserved1;
  ULONG  uCTAInUse;              // 0=CTB, 1=CTA - shows which CT Bus the card
                                 // has selected for the slave clock source
  ULONG  uReserved2[15]; 
} FS_CLOCKING_STATUS, *PFS_CLOCKING_STATUS;
#pragma pack(pop)

//######################################################################################################
//
// FarSync Error Return Codes
//
// These return codes will be passed back from the FarSync SDCI driver in the IoStatus.Status field of 
// the IRP and (if the SDCI client is a user-mode app) will be passed through transparently to the app 
// via GetLastError().
//

#define FS_ERROR_INVALID_INPUT_BUFFER_LENGTH  0xE0000001
#define FS_ERROR_INVALID_OUTPUT_BUFFER_LENGTH 0xE0000002
#define FS_ERROR_PRIMARY_IOCTL                0xE0000003
#define FS_ERROR_CARD_NOT_STARTED             0xE0000004
#define FS_ERROR_INVALID_CT_BUS_MODE          0xE0000005
#define FS_ERROR_INVALID_CT_BUS_FEED          0xE0000006
#define FS_ERROR_INVALID_IOCTL_FOR_PORTTYPE   0xE0000007
#define FS_ERROR_INVALID_LENGTH               0xE0000008

//######################################################################################################



#endif /* __SDCI_H_ */

