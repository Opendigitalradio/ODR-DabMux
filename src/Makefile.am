# Copyright (C) 2008, 2009,2012 Her Majesty the Queen in Right of Canada
# (Communications Research Center Canada)
# Pascal Charest
#
# Copyright (C) 2014, Matthias P. Braendli, http://mpb.li
#

# This file is part of ODR-DabMux.
# 
# ODR-DabMux is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
# 
# ODR-DabMux is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with ODR-DabMux.  If not, see <http://www.gnu.org/licenses/>.

if IS_GIT_REPO
GITVERSION_FLAGS = -DGITVERSION="\"`git describe --dirty`\""
else
GITVERSION_FLAGS =
endif

FEC_FLAGS   =
FEC_LIBS    =-lfec

if HAVE_ZEROMQ_TEST
bin_PROGRAMS=odr-dabmux odr-bridgetest zmqinput-keygen

if HAVE_OUTPUT_RAW_TEST
bin_PROGRAMS+=odr-zmq2farsync
endif

ZMQ_LIBS    =-lzmq
else
bin_PROGRAMS=odr-dabmux odr-bridgetest

ZMQ_LIBS    =
endif

if HAVE_CURL_TEST
CURL_LIBS    =-lcurl
else
CURL_LIBS    =
endif

odr_dabmux_CFLAGS   =-Wall -I$(FARSYNC_DIR) $(GITVERSION_FLAGS)
odr_dabmux_CXXFLAGS =-Wall -I$(FARSYNC_DIR) $(GITVERSION_FLAGS)
odr_dabmux_LDADD    =$(FEC_LIBS) $(ZMQ_LIBS) $(CURL_LIBS) \
					 -lpthread -lboost_thread -lboost_system -lboost_regex
odr_dabmux_SOURCES  =DabMux.cpp DabMux.h \
					 DabMultiplexer.cpp DabMultiplexer.h \
                     dabInput.h dabInput.cpp \
                     dabInputBridgeUdp.h dabInputBridgeUdp.cpp \
                     dabInputDabplusFifo.h dabInputDabplusFifo.cpp \
                     dabInputDabplusFile.h dabInputDabplusFile.cpp \
                     dabInputDmbFile.h dabInputDmbFile.cpp \
                     dabInputDmbUdp.h dabInputDmbUdp.cpp \
                     dabInputEnhancedFifo.h dabInputEnhancedFifo.cpp \
                     dabInputEnhancedPacketFile.h dabInputEnhancedPacketFile.cpp \
                     dabInputFifo.h dabInputFifo.cpp \
                     dabInputFile.h dabInputFile.cpp \
                     dabInputMpegFifo.h dabInputMpegFifo.cpp \
                     dabInputMpegFile.h dabInputMpegFile.cpp \
                     dabInputPacketFile.h dabInputPacketFile.cpp \
                     dabInputPrbs.h dabInputPrbs.cpp \
                     dabInputRawFile.h dabInputRawFile.cpp \
                     dabInputRawFifo.h dabInputRawFifo.cpp \
                     dabInputSlip.h dabInputSlip.cpp \
                     dabInputTest.h dabInputTest.cpp \
                     dabInputUdp.h dabInputUdp.cpp \
                     dabInputZmq.h dabInputZmq.cpp \
                     dabOutput/dabOutput.h \
                     dabOutput/dabOutputFile.cpp \
                     dabOutput/dabOutputFifo.cpp \
                     dabOutput/dabOutputRaw.cpp \
                     dabOutput/dabOutputSimul.cpp \
                     dabOutput/dabOutputTcp.cpp \
                     dabOutput/dabOutputUdp.cpp \
                     dabOutput/dabOutputZMQ.cpp \
                     dabOutput/edi/AFPacket.cpp dabOutput/edi/AFPacket.h \
                     dabOutput/edi/TagItems.cpp dabOutput/edi/TagItems.h \
                     dabOutput/edi/TagPacket.cpp dabOutput/edi/TagPacket.h \
                     dabOutput/edi/PFT.cpp dabOutput/edi/PFT.h \
                     ClockTAI.h ClockTAI.cpp \
                     ConfigParser.cpp ConfigParser.h \
                     Dmb.h Dmb.cpp \
                     Eti.h Eti.cpp \
                     InetAddress.h InetAddress.cpp \
                     Interleaver.h Interleaver.cpp \
                     Log.h Log.cpp \
                     ManagementServer.h ManagementServer.cpp \
                     MuxElements.cpp MuxElements.h \
                     ParserCmdline.cpp ParserCmdline.h \
                     PcDebug.h \
                     ReedSolomon.h ReedSolomon.cpp \
                     RemoteControl.cpp RemoteControl.h \
                     TcpServer.h TcpServer.cpp \
                     TcpSocket.h TcpSocket.cpp \
                     UdpSocket.h UdpSocket.cpp \
                     bridge.h bridge.c \
                     crc.h crc.c \
                     dabUtils.h dabUtils.cpp \
                     fig/FIG.h fig/FIG.cpp \
                     fig/FIG0.cpp fig/FIG0.h \
                     fig/FIG1.cpp fig/FIG1.h \
                     fig/FIGCarousel.cpp fig/FIGCarousel.h \
                     mpeg.h mpeg.c \
                     prbs.h prbs.c \
                     utils.cpp utils.h \
                     zmq.hpp

odr_bridgetest_CFLAGS   =-DBRIDGE_TEST
odr_bridgetest_SOURCES  =bridge.c \
                         crc.c crc.h

zmqinput_keygen_SOURCES  = zmqinput-keygen.c
zmqinput_keygen_LDADD    = $(ZMQ_LIBS)
zmqinput_keygen_CFLAGS   = -Wall $(GITVERSION_FLAGS)

odr_zmq2farsync_SOURCES  = zmq2farsync/zmq2farsync.cpp \
                           dabOutput/dabOutput.h \
                           dabOutput/dabOutputRaw.cpp \
                           Log.h Log.cpp \
                           zmq.hpp
odr_zmq2farsync_LDADD    = $(ZMQ_LIBS)
odr_zmq2farsync_CFLAGS   = -Wall $(GITVERSION_FLAGS) -I$(FARSYNC_DIR)
odr_zmq2farsync_CXXFLAGS = -Wall $(GITVERSION_FLAGS) -I$(FARSYNC_DIR)
