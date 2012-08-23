# Copyright (C) 2008, 2009,2012 Her Majesty the Queen in Right of Canada
# (Communications Research Center Canada)
#
# Pascal Charest and 
# Matthias P. Braendli

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

bin_PROGRAMS=CRC-DabMux CRC-BridgeTest

FEC_FLAGS   =
FEC_LIBS    =-lfec

CRC_DabMux_CPPFLAGS =-I$(FARSYNC_DIR)
CRC_DabMux_LDADD    =$(FEC_LIBS) -lpthread
CRC_DabMux_SOURCES  =DabMux.cpp \
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
                     dabOutput.h dabOutput.cpp \
                     bridge.h bridge.c \
                     utils.cpp utils.h \
                     MuxElements.cpp MuxElements.h \
                     ParserCmdline.cpp ParserCmdline.h \
                     ParserConfigfile.cpp ParserConfigfile.h \
                     Eti.h Eti.cpp \
                     TcpLog.h TcpLog.cpp \
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
                     TcpServer.h TcpServer.cpp \
                     TcpSocket.h TcpSocket.cpp

CRC_BridgeTest_CFLAGS   =-DBRIDGE_TEST
CRC_BridgeTest_SOURCES  =bridge.c \
                         crc.c crc.h

CRC-DabMux-cfg: CRC-DabMux
	@echo "Copying CRC-DabMux to CRC-DabMux-cfg that reads ensemble config file"
	cp CRC-DabMux CRC-DabMux-cfg

.PHONY: CRC-DabMux-cfg

all: CRC-DabMux-cfg all-am

clean-local:
	rm CRC-DabMux-cfg
