# Copyright (C) 2008, 2009,2012 Her Majesty the Queen in Right of Canada
# (Communications Research Center Canada)
# Pascal Charest
#
# Copyright (C) 2014, Matthias P. Braendli, http://mpb.li
#

# This file is part of CRC-DabMux.
# 
# CRC-DabMux is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
# 
# CRC-DabMux is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with CRC-DabMux.  If not, see <http://www.gnu.org/licenses/>.

bin_PROGRAMS=crc-dabmux crc-bridgetest

if IS_GIT_REPO
GITVERSION_FLAGS = -DGITVERSION="\"-`git describe --long --tags`\""
else
GITVERSION_FLAGS = -DGITVERSION="\"-exported\""
endif

FEC_FLAGS   =
FEC_LIBS    =-lfec

if HAVE_ZEROMQ_TEST
ZMQ_LIBS    =-lzmq
else
ZMQ_LIBS    =
endif

crc_dabmux_CPPFLAGS =-I$(FARSYNC_DIR) $(GITVERSION_FLAGS)
crc_dabmux_LDADD    =$(FEC_LIBS) $(ZMQ_LIBS) -lpthread -lboost_thread -lboost_system
crc_dabmux_SOURCES  =DabMux.cpp \
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
                     bridge.h bridge.c \
                     utils.cpp utils.h \
                     MuxElements.cpp MuxElements.h \
                     RemoteControl.cpp RemoteControl.h \
                     ParserCmdline.cpp ParserCmdline.h \
                     ParserConfigfile.cpp ParserConfigfile.h \
                     Eti.h Eti.cpp \
                     Log.h Log.cpp \
                     UdpSocket.h UdpSocket.cpp \
                     InetAddress.h InetAddress.cpp \
                     prbs.h prbs.c \
                     crc.h crc.c \
                     dabUtils.h dabUtils.cpp \
                     PcDebug.h \
                     Dmb.h Dmb.cpp \
                     Interleaver.h Interleaver.cpp \
                     ReedSolomon.h ReedSolomon.cpp \
                     mpeg.h mpeg.c \
                     StatsServer.h StatsServer.cpp \
                     TcpServer.h TcpServer.cpp \
                     TcpSocket.h TcpSocket.cpp

crc_bridgetest_CFLAGS   =-DBRIDGE_TEST
crc_bridgetest_SOURCES  =bridge.c \
                         crc.c crc.h

