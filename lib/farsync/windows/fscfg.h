/*******************************************************************************
*                                                                 
* Program     : FarSync (generic)
*                                                                 
* File        : fscfg.h
*                                                                 
* Description : Common FarSync Configuration Definitions used through the 
*               FarSync PC-based  modules
*                                                    
* Modifications
*
* Version 2.1.0  01Mar01  WEB  Add device and port configuration values 
*                09Mar01  WEB  Add Fswan hwid
* Version 2.2.0  18Jul01  WEB  Certification candidate
*                22Oct01  MJD  Added Transparent Mode support - define 
*                              names and values for Transparent Mode and
*                              Buffer Length parameters.
* Version 2.2.6  26Oct01  WEB  Add name for devices use only for SDCI applications.
* Version 2.3.0  28Nov01  WEB  Add minnow ids + DeviceIDToCardID map
* Version 2.3.1  08Apr02  WEB  Change min buffer length from 62 to 2
* Version 2.3.2  27Aug02  WEB  Rationalised
* Version 3.0.2  05Nov02  WEB  Add 'U' to DeviceIDToCardID map
* Version 3.2.0  09Dec02  WEB  Add Invert Rx Clock, Dual Clocking & DMA definitions
* Version 3.4.0  26Oct04  WEB  Add fstap definitions
* Version 3.5.0  11Nov04  WEB  Add te1 definitions
* Version 4.0.0  18Feb05  WEB  Add configurable number and size of buffers, start 
*                              tx/rx
* Version 4.0.1  03Mar05  WEB  Add extended clocking definitions
* Version 4.0.2  17Mar05  WEB  Correct FS_LINE_INTERFACE_MAX - should be V35
* Version 4.1.0  24Jun05  WEB  Add new T4E defs
* Version 4.2.0  12Sep05  WEB  Add FS_NDEVICE_CLASS i.e. support for T4E MkII
*                              Reduce FS_LINE_RATE_MIN to 300
*
*******************************************************************************/

#ifndef __FSCFG_H_
#define __FSCFG_H_

#define SMCUSER_PACKING 1
#include "smcuser.h"  // Values also used in the adapter window interface */

// PCI IDs
#define FS_VENDOR_ID 0x1619
#define FS_DEVICE_CLASS 0x0400  // ie. 0400 .. 04FF
#define FS_TDEVICE_CLASS FS_DEVICE_CLASS

// FarSync card classes/Device ID ranges
#define FS_MDEVICE_CLASS 0x0500  // ie. 0500 .. 05FF
#define FS_VDEVICE_CLASS 0x0000  // ie. 0000 .. 00FF
#define FS_UDEVICE_CLASS 0x0600  // ie. 0600 .. 06FF
#define FS_EDEVICE_CLASS 0x1600  // ie. 1600 .. 16FF
#define FS_NDEVICE_CLASS 0x3600  // ie. 3600 .. 36FF
#define FS_DDEVICE_CLASS 0x1700  // ie. 1700 .. 17FF

#define FS_CARDID_MASK  0xFF00

// Class maps
struct _DEVICEIDTOCARDID// e.g. 0400 -> T, 0500 -> M, 0600 -> T
{
  ULONG uDeviceID;
  char  cCardID;
};
#define DEVICEIDTOCARDID struct _DEVICEIDTOCARDID
#ifdef FS_DECLARE_CFG_ARRAYS
DEVICEIDTOCARDID DeviceIDToCardID[] = { {FS_TDEVICE_CLASS, 'T'},
                                        {FS_MDEVICE_CLASS, 'M'},
                                        {FS_VDEVICE_CLASS, 'V'},
                                        {FS_UDEVICE_CLASS, 'T'},
                                        {FS_EDEVICE_CLASS, 'T'},
                                        {FS_DDEVICE_CLASS, 'D'},
                                        {FS_NDEVICE_CLASS, 'T'}
                                      };
typedef struct _DEVICEIDTOCARDID2
{
  ULONG uDeviceID;
  char  cCardID;
} DEVICEIDTOCARDID2;  // e.g. 0400 -> P, 0500 -> P, 0600 -> U
#endif

#ifdef FS_DECLARE_CFG_ARRAYS
DEVICEIDTOCARDID DeviceIDToCardID2[] = { {FS_TDEVICE_CLASS, 'P'},
                                         {FS_MDEVICE_CLASS, 'P'},
                                         {FS_VDEVICE_CLASS, 'P'},
                                         {FS_UDEVICE_CLASS, 'U'},
                                         {FS_EDEVICE_CLASS, 'E'},
                                         {FS_DDEVICE_CLASS, 'S'},
                                         {FS_NDEVICE_CLASS, 'E'}
                                       };
#else
DEVICEIDTOCARDID DeviceIDToCardID2[];
#endif

#define FS_FSWAN_HWID  "fsvbus\\FS_TXP-00"   // must match value in .inf
#define FS_VBUS_PREFIX_U L"fsv"

// Limits required for adapter/port enumeration
#define FS_MAX_ADAPTERS      100
#define FS_MAX_SYNC_ADAPTERS  10
#define FS_MAX_PORTS          MAX_PORTS

#define FS_MAX_DEVICE_NAME  32 // number of WCHARs required to accomodate the longest device name 
                               // e.g. \Device\SYNCH, \DosDevices\SYNC1 or \DosDevices\FSPPP0006
#define FS_DEVICE_PREFIX     "SYNC"
#define FS_DEVICE_PREFIX_U  L"SYNC"

#define FS_PPP_DEVICE_NAME  "FSPPP%s"  // name of farsynct device when in ppp mode

#define FS_SDCI_DEVICE_NAME  "SDCI%s"  // name of farsynct device when in sdci mode

#define FS_PORT_DEVICE_NAME   "PortDeviceName"  // stores name of farsynct port to use
#define LFS_PORT_DEVICE_NAME  L"PortDeviceName" // required for use in ndiswan driver

#define FS_MAX_PARAM_NAME 128  // max number of chars in FS_LINE_NAME_PARAM_NAME etc.

/* Line name values */
#define FS_LINE_NAME_PARAM_NAME   "Port%u_%u_Name"
#define FS_LINE_NAME_MAX_CHARS    16
#define FS_LINE_NAME_DEF          "FSWAN_%u"
#define FS_PORT_NAME_DEF          "Port %c"
#define FS_PORT_NAME_PARAM_NAME   "Port%u_Name"

/* Line interface values */
#define FS_LINE_INTERFACE_PARAM_NAME    "Port%u_%u_Interface"
#define FS_LINE_INTERFACE_MIN           V24
#define FS_LINE_INTERFACE_V24           V24
#define FS_LINE_INTERFACE_X21           X21
#define FS_LINE_INTERFACE_X21D          X21D
#define FS_LINE_INTERFACE_V35           V35
#define FS_LINE_INTERFACE_RS530_449     RS530_449
// Note: Specifying MAX as X21D intentionally makes RS530_449 not accessible to the current client GUIs
#define FS_LINE_INTERFACE_MAX           RS530_449
#define FS_LINE_INTERFACE_DEF           X21    
#define FS_LINE_INTERFACE_MAX_CHARS     7
#define FS_PORT_INTERFACE_PARAM_NAME    "Port%u_Interface"
#define FS_PORT_INTERFACE_PARAM_NAME_U L"Port%u_Interface"

#ifdef FS_DECLARE_CFG_ARRAYS
char* szFsT4EInterfaceTypesTable[]    = {"Auto", "V.24", "X.21", "V.35", "X.21 (Dual-Clocking)", "RS530/449", 0};
char* szFsT4EInterfaceTypesTable2[]   = {"Auto", "V.24", "X.21", "V.35", "X.21D", "RS530", 0};
ULONG uFsT4EInterfaceTypesTableVals[] = {AUTO, V24, X21, V35, X21D, RS530_449, 0};
char* szFsInterfaceTypesTable[]       = {"Auto", "V.24", "X.21", "V.35", "X.21 (Dual-Clocking)", 0};
char* szFsM1PInterfaceTypesTable[]    = {"Auto", "V.24", "X.21", "V.35", 0};
#else
char* szFsT4EInterfaceTypesTable[];
char* szFsT4EInterfaceTypesTable2[];
ULONG uFsT4EInterfaceTypesTableVals[];
char* szFsInterfaceTypesTable[];
char* szFsM1PInterfaceTypesTable[];
#endif

/* Line rate values */
#define FS_LINE_RATE_PARAM_NAME    "Port%u_%u_Rate"
#define FS_LINE_RATE_MIN           300
#define FS_LINE_RATE_MAX           8192000
#define FS_LINE_RATE_DEF           64000
#define FS_LINE_RATE_MAX_CHARS     7
#define FS_PORT_RATE_PARAM_NAME    "Port%u_Rate"
#define FS_PORT_RATE_PARAM_NAME_U L"Port%u_Rate"

#ifdef FS_DECLARE_CFG_ARRAYS
ULONG uFsRateTable[] = {1200,     2400,    4800,    9600,    
                        19200,    38400,   64000,   76800,  
                        128000,   153600,  256000,  307200,
                        512000,   614400,  1024000, 2048000,        
                        4096000,  8192000, 0 };
#else
ULONG uFsRateTable[];
#endif

/* Line clocking values */
#define FS_LINE_CLOCKING_PARAM_NAME    "Port%u_%u_InternalClocking"
#define FS_LINE_CLOCKING_MIN           EXTCLK
#define FS_LINE_CLOCKING_EXTERNAL      EXTCLK
#define FS_LINE_CLOCKING_INTERNAL      INTCLK
#define FS_LINE_CLOCKING_MAX           INTCLK
#define FS_LINE_CLOCKING_DEF           EXTCLK
#define FS_LINE_CLOCKING_MAX_CHARS     8
#define FS_PORT_CLOCKING_PARAM_NAME    "Port%u_InternalClocking"
#define FS_PORT_CLOCKING_PARAM_NAME_U L"Port%u_InternalClocking"

#ifdef FS_DECLARE_CFG_ARRAYS
char* szFsClockingTypesTable[] = {"External", "Internal", 0}; // maps onto FALSE, TRUE
#else
char* szFsClockingTypesTable[];
#endif

/* Transparent mode values */
#define FS_LINE_TRANSPARENT_MIN           FALSE
#define FS_LINE_TRANSPARENT_MAX           TRUE
#define FS_LINE_TRANSPARENT_DEF           FALSE
#define FS_PORT_TRANSPARENT_PARAM_NAME    "Port%u_TransparentMode"
#define FS_PORT_TRANSPARENT_PARAM_NAME_U L"Port%u_TransparentMode"

/* HDLC/Transparent mode values */
#define FS_LINE_MODE_HDLC          0
#define FS_LINE_MODE_TRANSPARENT   1
#define FS_LINE_MODE_MIN           FS_LINE_MODE_HDLC          
#define FS_LINE_MODE_MAX           FS_LINE_MODE_TRANSPARENT
#define FS_LINE_MODE_DEF           FS_LINE_MODE_HDLC
#define FS_LINE_MODE_MAX_CHARS     12

#ifdef FS_DECLARE_CFG_ARRAYS
char* szFsModeTypesTable[] = {"HDLC", "Transparent", 0};
#else
char* szFsModeTypesTable[];
#endif

/* Buffer length values */
#define FS_LINE_BUFFER_MIN           2
#define FS_LINE_BUFFER_MAX           64*1024
#define FS_LINE_BUFFER_DEF           8*1024
#define FS_PORT_BUFFER_PARAM_NAME       "Port%u_BufferLength"
#define FS_PORT_BUFFER_PARAM_NAME_U    L"Port%u_BufferLength"
#define FS_PORT_TX_BUFFER_PARAM_NAME    "Port%u_TxBufferLength"
#define FS_PORT_TX_BUFFER_PARAM_NAME_U L"Port%u_TxBufferLength"
#define FS_PORT_RX_BUFFER_PARAM_NAME    "Port%u_RxBufferLength"
#define FS_PORT_RX_BUFFER_PARAM_NAME_U L"Port%u_RxBufferLength"

// Note: UNDEF values means that the vars are ignored (i.e. different from DEFault values)

/* Invert Rx clock values */
#define FS_PORT_INVERT_RX_CLOCK_MIN           FALSE
#define FS_PORT_INVERT_RX_CLOCK_MAX           TRUE
#define FS_PORT_INVERT_RX_CLOCK_DEF           FALSE
#define FS_PORT_INVERT_RX_CLOCK_UNDEF         0xFFFFFFFF
#define FS_PORT_INVERT_RX_CLOCK_PARAM_NAME    "Port%u_InvertRxClock"
#define FS_PORT_INVERT_RX_CLOCK_PARAM_NAME_U L"Port%u_InvertRxClock"

/* Dual Clocking values */
#define FS_PORT_DUAL_CLOCKING_DEF           FALSE
#define FS_PORT_DUAL_CLOCKING_PARAM_NAME    "Port%u_DualClocking"
#define FS_PORT_DUAL_CLOCKING_PARAM_NAME_U L"Port%u_DualClocking"

/* Tx DMA values */
#define FS_PORT_TX_DMA_UNDEF         0xFFFFFFFF
#define FS_PORT_TX_DMA_PARAM_NAME    "Port%u_TxDMA"
#define FS_PORT_TX_DMA_PARAM_NAME_U L"Port%u_TxDMA"

/* Rx DMA values */
#define FS_PORT_RX_DMA_UNDEF         0xFFFFFFFF
#define FS_PORT_RX_DMA_PARAM_NAME    "Port%u_RxDMA"
#define FS_PORT_RX_DMA_PARAM_NAME_U L"Port%u_RxDMA"

#define FS_PORT_DMA_MAX FSDMAMODE_ON
#define FS_PORT_DMA_MIN FSDMAMODE_OFF
#define FS_PORT_DMA_DEF FSDMAMODE_OFF

/* Number of Tx buffers */
#define FS_PORT_TX_NUM_BUFFERS_MIN           2
#define FS_PORT_TX_NUM_BUFFERS_MAX           128
#define FS_PORT_TX_NUM_BUFFERS_DEF           8
#define FS_PORT_TX_NUM_BUFFERS_UNDEF         0xFFFFFFFF
#define FS_PORT_TX_NUM_BUFFERS_PARAM_NAME    "Port%u_TxNumBuffers"
#define FS_PORT_TX_NUM_BUFFERS_PARAM_NAME_U L"Port%u_TxNumBuffers"

/* Number of Rx buffers */
#define FS_PORT_RX_NUM_BUFFERS_MIN           2
#define FS_PORT_RX_NUM_BUFFERS_MAX           128
#define FS_PORT_RX_NUM_BUFFERS_DEF           8
#define FS_PORT_RX_NUM_BUFFERS_UNDEF         0xFFFFFFFF
#define FS_PORT_RX_NUM_BUFFERS_PARAM_NAME    "Port%u_RxNumBuffers"
#define FS_PORT_RX_NUM_BUFFERS_PARAM_NAME_U L"Port%u_RxNumBuffers"

#ifdef FS_DECLARE_CFG_ARRAYS
char* szFsNumBuffersTable[] = {"2", "4", "8", "16", "32", "64", "128", 0};
#else
char* szFsNumBuffersTable[];
#endif

/* Encoding values */
#define FS_PORT_ENCODING_PARAM_NAME     "Port%u_Encoding"
#define FS_PORT_ENCODING_PARAM_NAME_U  L"Port%u_Encoding"
#define FS_PORT_ENCODING_NRZ  0x80
#define FS_PORT_ENCODING_NRZI 0xa0
#define FS_PORT_ENCODING_FM0  0xc0
#define FS_PORT_ENCODING_FM1  0xd0
#define FS_PORT_ENCODING_MAN  0xe0
#define FS_PORT_ENCODING_MIN  FS_PORT_ENCODING_NRZ
#define FS_PORT_ENCODING_MAX  FS_PORT_ENCODING_MAN
#define FS_PORT_ENCODING_DEF  FS_PORT_ENCODING_NRZ

#ifdef FS_DECLARE_CFG_ARRAYS
char* szFsEncodingTypesTable[] = {"NRZ", "NRZI", "FM0", "FM1", "MAN", 0};
#else
char* szFsEncodingTypesTable[];
#endif


/* Registry Config Names */
#define FS_EVENT_SUBKEY_NAME      "SYSTEM\\CurrentControlSet\\Services\\EventLog\\System\\farsynct"
#define EVENT_VAR_TYPES_SUPPORTED "TypesSupported"
#define EVENT_VAR_MESSAGE_FILE    "EventMessageFile"
#define FS_MESSAGE_FILE           "%SystemRoot%\\System32\\IOLOGMSG.DLL;%SystemRoot%\\System32\\Drivers\\farsynct.sys"

#define REG_SERVICES_ROOT_U       L"\\REGISTRY\\Machine\\SYSTEM\\CurrentControlSet\\Services\\"
#define REG_MACHINE_ROOT_U        L"\\REGISTRY\\Machine\\"
#define REG_FSWAN_CONFIG_PATH_U   L"SOFTWARE\\FarSite\\FSWAN"

/* Pool Tags */

#define FARSYNCT_TAG1  '1TsF'
#define FARSYNCM_TAG1  '1PsF'

#define FSX25MDM_TAG1  '1MsF'

#define FSKUTL_TAG1 '1UsF'

#define FSWAN_TAG1  '1WsF'
#define FSWAN_TAG2  '2WsF'

/* fstap config */

#define FSTAP_DEVICENAME_PARAM_NAME        "DeviceName"
#define FSTAP_PORT_PARAM_NAME              "Port"
#define FSTAP_IGNORE_SIGNALS_DEF           FALSE
#define FSTAP_IGNORE_SIGNALS_PARAM_NAME    "IgnoreSignals"
#define FSTAP_ENABLE_MONITORING_DEF        TRUE
#define FSTAP_ENABLE_MONITORING_PARAM_NAME "EnableMonitoring"

/* te1 config */

#define FS_PORT_FRAMING_MIN           FRAMING_E1
#define FS_PORT_FRAMING_MAX           FRAMING_T1
#define FS_PORT_FRAMING_PARAM_NAME    "Port%u_Framing"
#define FS_PORT_FRAMING_PARAM_NAME_U  L"Port%u_Framing"

#ifdef FS_DECLARE_CFG_ARRAYS
char* szFsFramingTypesTable[] = {"E1", "J1", "T1", 0};
#else
char* szFsFramingTypesTable[];
#endif

#define FS_PORT_ECLOCKING_MIN           CLOCKING_SLAVE
#define FS_PORT_ECLOCKING_MAX           CLOCKING_MASTER
#ifdef notreq
#define FS_PORT_ECLOCKING_PARAM_NAME    "Port%u_Clocking"   
#define FS_PORT_ECLOCKING_PARAM_NAME_U  L"Port%u_Clocking"   
#endif

#ifdef FS_DECLARE_CFG_ARRAYS
char* szFsEClockingTypesTable[] = {"Slave", "Master", 0};
#else
char* szFsEClockingTypesTable[];
#endif

#define FS_PORT_DATARATE_MIN              8000
#define FS_PORT_E1_DATARATE_MAX           2048000
#define FS_PORT_T1_DATARATE_MAX           1544000
#ifdef notreq
#define FS_PORT_DATARATE_PARAM_NAME    "Port%u_DataRate"   
#define FS_PORT_DATARATE_PARAM_NAME_U  L"Port%u_DataRate"   

#ifdef FS_DECLARE_CFG_ARRAYS
char* szFsDataRateTable[] = {"1", "2", 0};
#else
char* szFsDataRateTable[];
#endif
#endif

#define FS_PORT_STRUCTURE_MIN           STRUCTURE_UNFRAMED
#define FS_PORT_STRUCTURE_MAX           STRUCTURE_T1_72
#define FS_PORT_STRUCTURE_PARAM_NAME    "Port%u_Structure"   
#define FS_PORT_STRUCTURE_PARAM_NAME_U  L"Port%u_Structure"   


#define STRUCTURE_UNFRAMED   0
#define STRUCTURE_E1_DOUBLE  1
#define STRUCTURE_E1_CRC4    2
#define STRUCTURE_E1_DEFAULT STRUCTURE_E1_CRC4
#define STRUCTURE_DEFAULT    STRUCTURE_E1_CRC4
#define STRUCTURE_E1_CRC4M   3
#define STRUCTURE_T1_4       4
#define STRUCTURE_T1_12      5
#define STRUCTURE_T1_24      6
#define STRUCTURE_T1_DEFAULT STRUCTURE_T1_24
#define STRUCTURE_T1_72      7

#ifdef FS_DECLARE_CFG_ARRAYS
char* szFsE1StructureTypesTable[] = {"Unframed", "Double", "CRC4", "CRC4M", 0};
int iFsE1StructureValuesTable[] = {STRUCTURE_UNFRAMED, STRUCTURE_E1_DOUBLE, STRUCTURE_E1_CRC4, STRUCTURE_E1_CRC4M, -1};
char* szFsT1StructureTypesTable[] = {"Unframed", "F4 (FT)", "F12 (D3/D4, SF)", "F24 (D5, Fe, ESF)", "F72 (SLC96)", 0};
int iFsT1StructureValuesTable[] = {STRUCTURE_UNFRAMED, STRUCTURE_T1_4, STRUCTURE_T1_12, STRUCTURE_T1_24, STRUCTURE_T1_72, -1};
#else
char* szFsE1StructureTypesTable[];
int iFsE1StructureValuesTable[];
char* szFsT1StructureTypesTable[];
int iFsT1StructureValuesTable[];
#endif

#define FS_PORT_IFACE_MIN           INTERFACE_RJ48C
#define FS_PORT_IFACE_MAX           INTERFACE_BNC
#define FS_PORT_IFACE_PARAM_NAME    "Port%u_Iface"   
#define FS_PORT_IFACE_PARAM_NAME_U  L"Port%u_Iface"   

#ifdef FS_DECLARE_CFG_ARRAYS
char* szFsE1IfaceTypesTable[] = {"RJ48C", "BNC", 0};
int iFsE1IfaceValuesTable[] = {INTERFACE_RJ48C, INTERFACE_BNC, -1};
char* szFsT1IfaceTypesTable[] = {"RJ48C", 0};
int iFsT1IfaceValuesTable[] = {INTERFACE_RJ48C, -1};
#else
char* szFsE1IfaceTypesTable[];
int iFsE1IfaceValuesTable[];
char* szFsT1IfaceTypesTable[];
int iFsT1IfaceValuesTable[];
#endif

#define FS_PORT_CODING_MIN           CODING_HDB3
#define FS_PORT_CODING_MAX           CODING_B8ZS
#define FS_PORT_CODING_PARAM_NAME    "Port%u_Coding"   
#define FS_PORT_CODING_PARAM_NAME_U  L"Port%u_Coding"   

#ifdef FS_DECLARE_CFG_ARRAYS
//char* szFsE1CodingTypesTable[] = {"HDB3", "NRZ", "CMI", "CMI-HDB3", "AMI", 0};
//int iFsE1CodingValuesTable[] = {CODING_HDB3, CODING_NRZ, CODING_CMI, CODING_CMI_HDB3, CODING_AMI, -1};
char* szFsE1CodingTypesTable[] = {"HDB3", "AMI", 0};
int iFsE1CodingValuesTable[] = {CODING_HDB3, CODING_AMI, -1};
//char* szFsT1CodingTypesTable[] = {"NRZ", "CMI", "CMI_B8ZS", "AMI", "AMI-ZCS", "B8ZS", 0};
//int iFsT1CodingValuesTable[] = {CODING_NRZ, CODING_CMI, CODING_CMI_B8ZS, CODING_AMI, CODING_AMI_ZCS, CODING_B8ZS, -1};
char* szFsT1CodingTypesTable[] = {"AMI", "AMI-ZCS", "B8ZS", 0};
int iFsT1CodingValuesTable[] = {CODING_AMI, CODING_AMI_ZCS, CODING_B8ZS, -1};
#else
char* szFsE1CodingTypesTable[];
int iFsE1CodingValuesTable[];
char* szFsT1CodingTypesTable[];
int iFsT1CodingValuesTable[];
#endif

#define FS_PORT_LBO_MIN           LBO_0dB
#define FS_PORT_LBO_MAX           LBO_22dB5
#define FS_PORT_LBO_PARAM_NAME    "Port%u_LBO"   
#define FS_PORT_LBO_PARAM_NAME_U  L"Port%u_LBO"   

#ifdef FS_DECLARE_CFG_ARRAYS
char* szFsLBOTypesTable[] = {"0", "7.5", "15", "22.5", 0};
#else
char* szFsLBOTypesTable[];
#endif

#define FS_PORT_EQUALIZER_MIN           EQUALIZER_SHORT
#define FS_PORT_EQUALIZER_MAX           EQUALIZER_LONG
#define FS_PORT_EQUALIZER_PARAM_NAME    "Port%u_Equalizer"   
#define FS_PORT_EQUALIZER_PARAM_NAME_U  L"Port%u_Equalizer"   

#ifdef FS_DECLARE_CFG_ARRAYS
char* szFsEqualizerTypesTable[] = {"Short", "Long", 0};
#else
char* szFsEqualizerTypesTable[];
#endif

#define FS_PORT_LOOP_MODE_MIN           LOOP_NONE
#define FS_PORT_LOOP_MODE_MAX           LOOP_REMOTE
#define FS_PORT_LOOP_MODE_PARAM_NAME    "Port%u_LoopMode"   
#define FS_PORT_LOOP_MODE_PARAM_NAME_U  L"Port%u_LoopMode"   

#ifdef FS_DECLARE_CFG_ARRAYS
char* szFsLoopModeTypesTable[] = {"None", "Local", "Payload (excl TS0)", "Payload (incl TS0)", "Remote", 0};
#else
char* szFsLoopModeTypesTable[];
#endif

#define FS_PORT_RANGE_MIN           RANGE_0_40_M
#define FS_PORT_RANGE_MAX           RANGE_162_200_M
#define FS_PORT_RANGE_PARAM_NAME    "Port%u_Range"   
#define FS_PORT_RANGE_PARAM_NAME_U  L"Port%u_Range"   

#ifdef FS_DECLARE_CFG_ARRAYS
char* szFsRangeTypesTable[] = {"0-133ft (0-40m)", "133-266ft (40-81m)", "266-399ft (81-122m)", "399-533ft (122-162m)", "533-655ft (162-200m)", 0};
#else
char* szFsRangeTypesTable[];
#endif

#define FS_PORT_BUFFER_MODE_MIN           BUFFER_2_FRAME
#define FS_PORT_BUFFER_MODE_MAX           BUFFER_NONE
#define FS_PORT_TX_BUFFER_MODE_PARAM_NAME    "Port%u_TxBufferMode"   
#define FS_PORT_TX_BUFFER_MODE_PARAM_NAME_U  L"Port%u_TxBufferMode"   
#define FS_PORT_RX_BUFFER_MODE_PARAM_NAME    "Port%u_RxBufferMode"   
#define FS_PORT_RX_BUFFER_MODE_PARAM_NAME_U  L"Port%u_RxBufferMode"   

#ifdef FS_DECLARE_CFG_ARRAYS
char* szFsBufferModeTypesTable[] = {"2 Frame", "1 Frame", "96 bit", "None", 0};
#else
char* szFsBufferModeTypesTable[];
#endif

#define FS_PORT_STARTING_TS_MIN           0
#define FS_PORT_STARTING_TS_MAX           31
#define FS_PORT_STARTING_TS_PARAM_NAME    "Port%u_StartingTs"   
#define FS_PORT_STARTING_TS_PARAM_NAME_U  L"Port%u_StartingTs"   

#ifdef FS_DECLARE_CFG_ARRAYS
char* szFsE1StartingTSTypesTable[] = { "1",  "2",  "3",  "4",  "5",  "6",  "7",  "8",  "9", 
                                    "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", 
                                    "20", "21", "22", "23", "24", "25", "26", "27", "28", "29", 
                                    "30", "31",
                                    0};
char* szFsT1StartingTSTypesTable[] = { "0",  "1",  "2",  "3",  "4",  "5",  "6",  "7",  "8",  "9", 
                                    "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", 
                                    "20", "21", "22", "23", 
                                    0};
#else
char* szFsE1StartingTSTypesTable[];
char* szFsT1StartingTSTypesTable[];
#endif

#define FS_PORT_LOS_THRESHOLD_MIN           0
#define FS_PORT_LOS_THRESHOLD_MAX           7
#define FS_PORT_LOS_THRESHOLD_PARAM_NAME    "Port%u_LOSThreshold"   
#define FS_PORT_LOS_THRESHOLD_PARAM_NAME_U  L"Port%u_LOSThreshold"   

#ifdef FS_DECLARE_CFG_ARRAYS
char* szFsLOSThresholdTypesTable[] = { "0",  "1",  "2",  "3",  "4",  "5",  "6",  "7",  0};
#else
char* szFsLOSThresholdTypesTable[];
#endif

#define FS_PORT_ENABLE_IDLE_CODE_MIN           0
#define FS_PORT_ENABLE_IDLE_CODE_MAX           1
#define FS_PORT_ENABLE_IDLE_CODE_PARAM_NAME    "Port%u_EnableIdleCode"   
#define FS_PORT_ENABLE_IDLE_CODE_PARAM_NAME_U  L"Port%u_EnableIdleCode"   

#define FS_PORT_IDLE_CODE_MIN           0
#define FS_PORT_IDLE_CODE_MAX           0xff
#define FS_PORT_IDLE_CODE_PARAM_NAME    "Port%u_IdleCode"   
#define FS_PORT_IDLE_CODE_PARAM_NAME_U  L"Port%u_IdleCode"   

#define FS_PORT_START_TXRX_MIN           0
#define FS_PORT_START_TXRX_DEF           START_TX_AND_RX
#define FS_PORT_START_TXRX_MAX           START_TX_AND_RX
#define FS_PORT_START_TXRX_PARAM_NAME    "Port%u_StartTxRx"   
#define FS_PORT_START_TXRX_PARAM_NAME_U  L"Port%u_StartTxRx"   

#define FS_PORT_CLOCK_SOURCE_MIN           FS_CLOCK_REFERENCE_OSCILLATOR
#define FS_PORT_CLOCK_SOURCE_DEF           FS_CLOCK_REFERENCE_OSCILLATOR
#define FS_PORT_CLOCK_SOURCE_MAX           FS_CLOCK_REFERENCE_CTBUS
#define FS_PORT_CLOCK_SOURCE_PARAM_NAME    "Port%u_ClockSource"   
#define FS_PORT_CLOCK_SOURCE_PARAM_NAME_U  L"Port%u_ClockSource"   

#ifdef FS_DECLARE_CFG_ARRAYS
char* szFsClockSourceTable[] = {"Local Oscillator", "CT_BUS", 0};
#else
char* szFsClockSourceTable[];
#endif

#define FS_SERIAL_EXTENDED_CLOCKING_DEF           FALSE
#define FS_SERIAL_EXTENDED_CLOCKING_PARAM_NAME    "Port%u_ExtendedClocking"   
#define FS_SERIAL_EXTENDED_CLOCKING_PARAM_NAME_U  L"Port%u_ExtendedClocking"

#define FS_SERIAL_INT_TX_CLOCK_DEF           FALSE
#define FS_SERIAL_INT_TX_CLOCK_PARAM_NAME    "Port%u_InternalTxClock"   
#define FS_SERIAL_INT_TX_CLOCK_PARAM_NAME_U  L"Port%u_InternalTxClock"

#define FS_SERIAL_INT_RX_CLOCK_DEF           FALSE
#define FS_SERIAL_INT_RX_CLOCK_PARAM_NAME    "Port%u_InternalRxClock"   
#define FS_SERIAL_INT_RX_CLOCK_PARAM_NAME_U  L"Port%u_InternalRxClock"

#define FS_SERIAL_TERM_TX_CLOCK_DEF           FALSE
#define FS_SERIAL_TERM_TX_CLOCK_PARAM_NAME    "Port%u_TerminalTxClock"   
#define FS_SERIAL_TERM_TX_CLOCK_PARAM_NAME_U  L"Port%u_TerminalTxClock"

#define FS_SERIAL_TERM_RX_CLOCK_DEF           FALSE
#define FS_SERIAL_TERM_RX_CLOCK_PARAM_NAME    "Port%u_TerminalRxClock"   
#define FS_SERIAL_TERM_RX_CLOCK_PARAM_NAME_U  L"Port%u_TerminalRxClock"

#define FS_SERIAL_DCD_OUTPUT_DEF           FALSE
#define FS_SERIAL_DCD_OUTPUT_PARAM_NAME    "Port%u_DCDOutput"   
#define FS_SERIAL_DCD_OUTPUT_PARAM_NAME_U  L"Port%u_DCDOutput"

#define FS_PORT_MSB_DEF              FALSE
#define FS_PORT_TX_MSB_PARAM_NAME    "Port%u_TxMSB"   
#define FS_PORT_TX_MSB_PARAM_NAME_U  L"Port%u_TxMSB"
#define FS_PORT_RX_MSB_PARAM_NAME    "Port%u_RxMSB"   
#define FS_PORT_RX_MSB_PARAM_NAME_U  L"Port%u_RxMSB"
   
#endif // __FSCFG_H_
