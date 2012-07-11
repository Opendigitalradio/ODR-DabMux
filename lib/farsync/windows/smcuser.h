/*******************************************************************************
*                                                                 
* Program     : FARSYNC 
*                                                                 
* File        : smcuser.h
*                                                                 
* Description : This common header file defines constants used throughout FarSync e.g. in
*
*                 1) onboard software
*                 2) PC driver (farsynct)
*                 3) user-mode config apps
*                 4) higher-level drivers (e.g. fswan)
*
* Modifications
*
* Version 2.0.0  18Jan01  WEB  Created
* Version 2.2.0  18Jul01  WEB  Certification candidate
*                22Oct01  MJD  Added Transparent Mode Support - moved number and
*                              size of buffer definitions her from smc.h.
*                19Nov02  MJD  Added X.21 Dual clock interface mode X21D - only
*                              for T1U/T2U/T4U.
*                28Nov02  MJD  Added NOCABLE interface mode - only for T1U/T2U/
*                              T4U/T4P, used only by CDE when stopping a port.
* Version 3.0.0  08Oct03  MJD  Added TE1 parameter constants
* Version 3.0.1  10Nov03  MJD  Added tx/rxBufferMode constants for TE1.
* Version 3.0.2  03Nov04  MJD  Added DSL #define's
* Version 3.0.3  24Feb05  WEB  Add FS_CLOCK_SOURCE_xxx and FS_CT_BUS_xxx values
* Version 4.1.0  15Jun05  WEB  Update TE1 typedefs
* Version 4.1.1  24Jun05  WEB  Add T4E MkII defs
* Version 4.1.2  30Jun05  MJD  Add FS_CT_BUS_FEED_FROM_OSCILLATOR define.
*                              Only define FS_CLOCK_SOURCE_PORT_A/B/C/D when
*                              T4EMKI defined (not available on T4EMKII).
* Version 4.1.3  07Jul05  MJD  Add FS_CONFIG_IN_USE_XXX and FS_INDICATION_CLOCK_XXX
*                              values for T4E MkII.
* Version 4.1.4  08Jul05  MJD  Added define for ANNEX_TYPE_AB (SHDSL)
* Version 4.1.5  14Jul05  WEB  Rationalise clock reference defines
* Version 4.1.6  15Jul05  MJD  Added FS_CCS_MASK_XXX defines
* Version 4.1.7  02Aug05  MJD  Added FS_INDICATION_CLOCK_SWITCH_TO_PRIMARY define
*******************************************************************************/

#ifndef SMCUSER_H
#define SMCUSER_H

#ifndef UINT8
#define UINT8  unsigned char
#define INT8   char
#define INT32  long
#define UINT16 unsigned short
#define UINT32 unsigned long
#endif

#define MAX_PORTS 4       /* maximum ports on T4P - fixed don't change*/

/* Interface Types  */

#define AUTO       0
#define V24        1
#define X21        2
#define V35        3
#define X21D       4
#define NOCABLE    5
#define RS530_449  6

/* Clock Sources  */

#define INTCLK 1
#define EXTCLK 0

#define FS_CLOCK_REFERENCE_OSCILLATOR    0  
#define FS_CLOCK_REFERENCE_PORT_A        1 
#define FS_CLOCK_REFERENCE_PORT_B        2 
#define FS_CLOCK_REFERENCE_PORT_C        3 
#define FS_CLOCK_REFERENCE_PORT_D        4 
#define FS_CLOCK_REFERENCE_CTBUS         5  /* note: this value does not differentiate between CTA or CTB */ 
#define FS_CLOCK_REFERENCE_CT_A          6 
#define FS_CLOCK_REFERENCE_CT_B          7 

#define FS_CT_BUS_MODE_SLAVE          0  /* FarSync card will act as SLAVE to the CT_BUS */
#define FS_CT_BUS_MODE_MASTER_A       1  /* FarSync card will act as MASTER to the CT_BUS_A */
#define FS_CT_BUS_MODE_MASTER_B       2  /* FarSync card will act as MASTER to the CT_BUS_B */
#define FS_CT_BUS_MODE_DEFAULT        FS_CT_BUS_MODE_SLAVE

#define FS_CONFIG_IN_USE_PRIMARY     0     /* Primary clock reference is in use            */
#define FS_CONFIG_IN_USE_BACKUP      1     /* Backup clock reference is in use             */
#define FS_CONFIG_IN_USE_OSCILLATOR  2     /* Local oscillator clock reference is in use   */
#define FS_CONFIG_IN_USE_DEFAULT     FS_CONFIG_IN_USE_PRIMARY

// Indications to be signalled via the cardNotifications FIFO

#define FS_INDICATION_CLOCK_RATE_CHANGED        0x0080 /* 2 LSB's denote port A, B, C or D */
#define FS_INDICATION_CLOCK_OUT_OF_TOLERANCE1   0x0084
#define FS_INDICATION_CLOCK_IN_TOLERANCE1       0x0088
#define FS_INDICATION_CLOCK_SWITCH_TO_BACKUP    0x008c
#define FS_INDICATION_CLOCK_OUT_OF_TOLERANCE2   0x0090
#define FS_INDICATION_CLOCK_IN_TOLERANCE2       0x0094
#define FS_INDICATION_CLOCK_SWITCH_TO_OSC       0x0098
#define FS_INDICATION_CLOCK_SWITCH_TO_CTA       0x009c
#define FS_INDICATION_CLOCK_SWITCH_TO_CTB       0x00a0
#define FS_INDICATION_CLOCK_SWITCH_TO_PRIMARY   0x00a4

// Bit masks for isolating fields in uCurrentStatusSummary

#define FS_CSS_MASK_SLAVE_MASTER  0x0100 /* 0 = Slave, 1 = Master              */
#define FS_CSS_MASK_CTA_CTB       0x0080 /* 0 = A, 1 = B                       */
#define FS_CSS_MASK_MASTER_SOURCE 0x0070 /* 000=A, 001=B, 010=C, 011=D, 100=LO */
#define FS_CSS_MASK_PORT_A_SOURCE 0x0008 /* 0 = CT, 1 = LO                     */
#define FS_CSS_MASK_PORT_B_SOURCE 0x0004 /* 0 = CT, 1 = LO                     */
#define FS_CSS_MASK_PORT_C_SOURCE 0x0002 /* 0 = CT, 1 = LO                     */
#define FS_CSS_MASK_PORT_D_SOURCE 0x0001 /* 0 = CT, 1 = LO                     */

/* Tx/Rx Start Parameters */

#define START_TX 1
#define START_RX 2
#define START_TX_AND_RX (START_TX | START_RX)
#define START_DEFAULT START_TX_AND_RX


// constants for t1/e1 service unit config
// =======================================
//
// dataRate
// rate is in bps: 8k - 1536/2048k (t1/e1), though n*64k is more usual
// e1: 0 - 1984k => framed (fractional), 2048k => unframed
// for e1 framed, bandwidth is allocated from the ninth bit of the frame
// sequentially
// for e1 unframed all 256 bits in the frame are used for data
// t1: 0 - 1536k => framed (fractional), no unframed mode in t1.
//

#define DATARATE_DEFAULT 64000L

// Clocking
// 

#define CLOCKING_SLAVE  0
#define CLOCKING_DEFAULT CLOCKING_SLAVE
#define CLOCKING_MASTER 1

// Framing
// 

#define FRAMING_E1      0
#define FRAMING_DEFAULT FRAMING_E1
#define FRAMING_J1      1 
#define FRAMING_T1      2 

// Structure
// 

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

// Interface
// RJ48C is available for e1 and t1, BNC is only available for e1
//

#define INTERFACE_RJ48C   0
#define INTERFACE_DEFAULT INTERFACE_RJ48C
#define INTERFACE_BNC     1

// Coding
// hdb3 is the normal coding scheme for e1
// b8zs is the normal coding scheme for t1, though ami is sometimes used
//

#define CODING_HDB3       0
#define CODING_E1_DEFAULT CODING_HDB3
#define CODING_DEFAULT    CODING_HDB3
#define CODING_NRZ        1
#define CODING_CMI        2
#define CODING_CMI_HDB3   3
#define CODING_CMI_B8ZS   4
#define CODING_AMI        5
#define CODING_AMI_ZCS    6
#define CODING_B8ZS       7
#define CODING_T1_DEFAULT CODING_B8ZS

// Line Build Out - for long haul t1 > 655ft (200m). Use with EQUALIZER_LONG.
// This parameter is ignored in e1 mode and t1/j1 short haul mode.
//

#define LBO_0dB     0
#define LBO_DEFAULT LBO_0dB
#define LBO_7dB5    1
#define LBO_15dB    2
#define LBO_22dB5   3

// Range - for short haul t1 < 655ft (200m). Use with EQUALIZER_SHORT.
// This parameter is ignored in e1 mode and t1/j1 long haul mode.
//

#define RANGE_0_133_FT   0
#define RANGE_DEFAULT    RANGE_0_133_FT
#define RANGE_0_40_M     RANGE_0_133_FT

#define RANGE_133_266_FT 1
#define RANGE_40_81_M    RANGE_133_266_FT

#define RANGE_266_399_FT 2
#define RANGE_81_122_M   RANGE_266_399_FT
 
#define RANGE_399_533_FT 3
#define RANGE_122_162_M  RANGE_399_533_FT

#define RANGE_533_655_FT 4
#define RANGE_162_200_M  RANGE_533_655_FT

// Receive Equalizer
// short haul -10dB
// long  haul -43dB (E1), -36dB (T1)
// only -36dB can be met with 1.2 silicon, requires equalizer parameter RAM
// changes for 2.1 silicon
//

#define EQUALIZER_SHORT   0
#define EQUALIZER_DEFAULT EQUALIZER_SHORT
#define EQUALIZER_LONG    1

// Loop Mode
// Local Loop transmits normally and loops PCM data on the line side of the
// framers.
// Payload Loop receives normally and loops TS0-TS31 or TS1-31 (with TS0
// regenerated by FALC-56) to line on the PCM side of the framers.
// Remote Loop receives normally and loops line data after clock recovery
// the optional Jitter Attenuation is currently not enabled.

#define LOOP_NONE            0
#define LOOP_DEFAULT LOOP_NONE
#define LOOP_LOCAL           1
#define LOOP_PAYLOAD_EXC_TS0 2
#define LOOP_PAYLOAD_INC_TS0 3
#define LOOP_REMOTE          4

// Buffer Mode
// buffer_none bypasses the elastic buffer
// 

#define BUFFER_2_FRAME 0
#define BUFFER_DEFAULT BUFFER_2_FRAME
#define BUFFER_1_FRAME 1
#define BUFFER_96_BIT  2
#define BUFFER_NONE    3

//
// Starting Timeslot (for fractional)
// E1 range 1 to 31, T1/J1 range 0 to 23
// Min/max values enforced by card. Actual speed may also be restricted if
// starting timeslot is too late in the frame.
// Parameter ignored in unchannelized mode
//

#define STARTING_DEFAULT 0

//
// LOS detection threshold
// level 0 allows LOS to be detected with larger  signal levels
// level 7 allows LOS to be detected with smaller signal levels
// the recommended default setting is level 2 - which is selected
// by the card if LOS_DEFAULT is configured
//

#define LOS_DEFAULT 0

#define LOS_LEVEL_0 1
#define LOS_LEVEL_1 2
#define LOS_LEVEL_2 3
#define LOS_LEVEL_3 4
#define LOS_LEVEL_4 5
#define LOS_LEVEL_5 6
#define LOS_LEVEL_6 7
#define LOS_LEVEL_7 8

#define LOS_SHORT 0x20
#define LOS_LONG  0x70

//
// Idle code for unused timeslots
//

#define IDLE_HDLC_FLAG 0x7e
#define IDLE_CODE_DEFAULT IDLE_HDLC_FLAG

#define TRANSPARENT_MODE_DEFAULT  FALSE
#define ENABLE_IDLE_CODE_DEFAULT  FALSE

// constants for dsl service unit config
// =======================================

//
// dataRate (G.SHDSL)
// rate is in bps: 192kbps (3B + 0Z) - 2312kbps (36B + 1Z)
// in 8kbps steps, where B is 64kbps and Z is 8kbps
//

//
// Terminal Type
//

#define TERMINAL_TYPE_REMOTE  0
#define TERMINAL_TYPE_CENTRAL 1

//
// Test Mode
//
// For User Diagnostics:
//
// Analog Transparent Loop     - loops from transmit line driver output to
//                               receive AGC input, bypassing the hybrid.
// Analog Non-Transparent Loop - loops transmit DAC back to receive AGC,
//                               bypassing transmit line driver and the hybrid.
//
// Others test modes are for Compliance Testing only
//

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
//
// Annex Type A (US) or B (EU)
//

#define ANNEX_TYPE_B 0
#define ANNEX_TYPE_DEFAULT ANNEX_TYPE_B
#define ANNEX_TYPE_A 1
#define ANNEX_TYPE_AB 2


/* Small Buffers are used only for diagnostics */

#define NUM_SMALL_TX_BUFFER 2
#define NUM_SMALL_RX_BUFFER 8

#define LEN_SMALL_TX_BUFFER 256 /* max size is 8192             */
#define LEN_SMALL_RX_BUFFER 256

/* Large Buffers are used for SDMA data */

// MAX_TX/RX_BUFFER determines, at compile time, the maximum number of 
// descriptors (hence buffers) per port for transmitter and receiver. Host can
// use any 2**N number of the descriptors between 1 and MAX_TX/RX_BUFFER, so
// some descriptors may be unused. Number of Buffers * Size of Buffers <=
// TX/RX_BUFFER_SPACE. Fewer buffers mean bigger buffers, more buffers mean
// smaller buffers.

#define MAX_TX_BUFFER 128
#define MAX_RX_BUFFER 128

// MAX_TX/RX_BUFFER_SPACE determines, at compile time, how much buffer space is
// available per port for transmitter and receiver. Buffer space is subdivided
// into 2**N buffers by host driver at runtime.
// Host can allocate 1 - MAX_TX/RX_BUFFER buffers.
// 0x10000 => 64KB for transmit, 64KB for receive; 4 ports => 512KB.
// MAX_TX/RX_BUFFER must be a 2**N multiple (for easy logic anlyser trigger).

#define TX_BUFFER_SPACE 0x10000
#define RX_BUFFER_SPACE 0x10000

/* Rx Descriptor bits */

#define FS_RX_DESC_ERR   0x4000
#define FS_RX_DESC_FRAM  0x2000
#define FS_RX_DESC_OFLO  0x1000
#define FS_RX_DESC_CRC   0x0800
#define FS_RX_DESC_HBUF  0x0400
#define FS_RX_DESC_ENP   0x0100

#define FS_RX_DESC_FRAM_ENP  0x2100
#define FS_RX_DESC_CRC_ENP   0x0900

#define NO_OF_DMA_CHANNELS 2

// The following structures require 2-byte packing.
// This file is used in a number of different environments.
// Windows SDCI applications should therefore include this file implicitly by explicitly 
// including fscfg.h which enables the packing directive to be used. 

#ifdef SMCUSER_PACKING

#if (SMCUSER_PACKING==1)
#pragma pack(push, 2)
#endif

typedef struct SU_CONFIG
{
  UINT32 dataRate;        // data rate in bps
  UINT8  clocking;        // master or slave
  UINT8  framing;         // E1, T1 or J1
  UINT8  structure;       // E1: unframed, double frame, CRC4; T1: F4, F12, F24 or F72
  UINT8  iface;           // RJ48C or BNC
  UINT8  coding;          // HDB3 or B8ZS + some other less used codes
  UINT8  lineBuildOut;    // 0, -7.5, -15, -22.5dB for t1 long haul only
  UINT8  equalizer;       // short or long haul settings
  UINT8  transparentMode; // FALSE (hdlc) or TRUE transparent data
  UINT8  loopMode;        // various local, payload and remote loops for test
  UINT8  range;           // 0-133, 133-266, 266-399, 399-533, 533-655ft for t1 short haul only
  UINT8  txBufferMode;    // transmit elastic buffer depth: 0 (bypass), 96 bits, 1 frame or 2 frame
  UINT8  rxBufferMode;    // receive elastic buffer depth:  0 (bypass), 96 bits, 1 frame or 2 frame
  UINT8  startingTimeSlot;// startingTimeSlot: E1 1-31, T1/J1 0-23
  UINT8  losThreshold;    // LOS Threshold: E1/T1/J1 0-7
  UINT8  enableIdleCode;  // Enable idle code for unused timeslots: TRUE, FALSE
  UINT8  idleCode;        // Idle code for unused timeslots: 0x00 - 0xff

  UINT8 spare[44];        // adjust to keep structure size 64 bytes
} FS_TE1_CONFIG, *PFS_TE1_CONFIG;

typedef struct SU_STATUS
{
  UINT32  receiveBufferDelay;    // delay through receive bufffer time slots (0-63)
  UINT32  framingErrorCounter;   // count of framing errors
  UINT32  codeViolationCounter;  // count of code violations
  UINT32  crcErrorCounter1;      // count of CRC errors
  INT32   lineAttenuation;       // receive line attenuation in dB, -ve => unknown
  BOOLEAN portStarted;           //
  BOOLEAN lossOfSignal;          // LOS alarm
  BOOLEAN receiveRemoteAlarm;    // RRA alarm
  BOOLEAN alarmIndicationSignal; // AIS alarm

  UINT8 spare[40];               // adjust to keep structure size 64 bytes
} FS_TE1_STATUS, *PFS_TE1_STATUS;

#if (SMCUSER_PACKING==1)
#pragma pack(pop)
#endif

#else
#pragma message("!!! *** SMCUSER_PACKING not defined. Note: Windows SDCI apps should  *** !!!")
#pragma message("!!! *** not include smcuser.h directly, use fscfg.h instead          *** !!!")
#endif 


#endif
