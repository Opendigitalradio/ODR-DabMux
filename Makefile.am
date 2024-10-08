# Copyright (C) 2008, 2009 Her Majesty the Queen in Right of Canada
# (Communications Research Center Canada)
#
# Copyright (C) 2024 Matthias P. Braendli
# http://opendigitalradio.org

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

bin_PROGRAMS=odr-dabmux zmqinput-keygen

if HAVE_OUTPUT_RAW_TEST
bin_PROGRAMS+=odr-zmq2farsync
endif

FARSYNC_DIR=lib/farsync/linux
INCLUDE=-I$(FARSYNC_DIR) -Ilib/charset -Ilib -Isrc

lib_fec_sources = lib/fec/char.h \
				  lib/fec/rs-common.h \
				  lib/fec/decode_rs_char.c \
				  lib/fec/decode_rs.h \
				  lib/fec/encode_rs_char.c \
				  lib/fec/encode_rs.h \
				  lib/fec/fec.h \
				  lib/fec/init_rs_char.c \
				  lib/fec/init_rs.h

lib_charset_sources = lib/charset/charset.cpp \
					  lib/charset/charset.h \
					  lib/charset/utf8/checked.h \
					  lib/charset/utf8/core.h \
					  lib/charset/utf8/unchecked.h \
					  lib/charset/utf8/cpp11.h \
					  lib/charset/utf8/cpp17.h \
					  lib/charset/utf8/cpp20.h \
					  lib/charset/utf8.h

odr_dabmux_CFLAGS   =-Wall $(INCLUDE) $(PTHREAD_CFLAGS) $(GITVERSION_FLAGS)
odr_dabmux_CXXFLAGS =-Wall $(PTHREAD_CXXFLAGS) $(INCLUDE) $(GITVERSION_FLAGS) $(BOOST_CPPFLAGS) $(ZMQ_CPPFLAGS)
odr_dabmux_LDADD    =$(ZMQ_LIBS) $(BOOST_LDFLAGS) \
					 $(PTHREAD_CFLAGS) $(PTHREAD_LIBS) $(BOOST_SYSTEM_LIB)

odr_dabmux_SOURCES  =src/DabMux.cpp \
					 src/DabMux.h \
					 src/DabMultiplexer.cpp \
					 src/DabMultiplexer.h \
					 src/input/inputs.h \
					 src/input/Prbs.cpp \
					 src/input/Prbs.h \
					 src/input/Zmq.cpp \
					 src/input/Zmq.h \
					 src/input/File.cpp \
					 src/input/File.h \
					 src/input/Udp.cpp \
					 src/input/Udp.h \
					 src/input/Edi.cpp \
					 src/input/Edi.h \
					 src/dabOutput/dabOutput.h \
					 src/dabOutput/dabOutputFile.cpp \
					 src/dabOutput/dabOutputFifo.cpp \
					 src/dabOutput/dabOutputRaw.cpp \
					 src/dabOutput/dabOutputSimul.cpp \
					 src/dabOutput/dabOutputTcp.cpp \
					 src/dabOutput/dabOutputUdp.cpp \
					 src/dabOutput/dabOutputZMQ.cpp \
					 src/dabOutput/metadata.h \
					 src/dabOutput/metadata.cpp \
					 src/ConfigParser.cpp \
					 src/ConfigParser.h \
					 src/Eti.h \
					 src/Eti.cpp \
					 src/ManagementServer.h \
					 src/ManagementServer.cpp \
					 src/MuxElements.cpp \
					 src/MuxElements.h \
					 src/PcDebug.h \
					 src/fig/FIG.h \
					 src/fig/FIG.cpp \
					 src/fig/FIG0.h \
					 src/fig/FIG0structs.h \
					 src/fig/FIG0_0.cpp \
					 src/fig/FIG0_0.h \
					 src/fig/FIG0_1.cpp \
					 src/fig/FIG0_1.h \
					 src/fig/FIG0_2.cpp \
					 src/fig/FIG0_2.h \
					 src/fig/FIG0_3.cpp \
					 src/fig/FIG0_3.h \
					 src/fig/FIG0_5.cpp \
					 src/fig/FIG0_5.h \
					 src/fig/FIG0_6.cpp \
					 src/fig/FIG0_6.h \
                                         src/fig/FIG0_7.cpp \
                                         src/fig/FIG0_7.h \
					 src/fig/FIG0_8.cpp \
					 src/fig/FIG0_8.h \
					 src/fig/FIG0_9.cpp \
					 src/fig/FIG0_9.h \
					 src/fig/FIG0_10.cpp \
					 src/fig/FIG0_10.h \
					 src/fig/FIG0_13.cpp \
					 src/fig/FIG0_13.h \
					 src/fig/FIG0_14.cpp \
					 src/fig/FIG0_14.h \
					 src/fig/FIG0_17.cpp \
					 src/fig/FIG0_17.h \
					 src/fig/FIG0_18.cpp \
					 src/fig/FIG0_18.h \
					 src/fig/FIG0_19.cpp \
					 src/fig/FIG0_19.h \
					 src/fig/FIG0_21.cpp \
					 src/fig/FIG0_21.h \
					 src/fig/FIG0_24.cpp \
					 src/fig/FIG0_24.h \
					 src/fig/FIG1.cpp \
					 src/fig/FIG1.h \
					 src/fig/FIG2.cpp \
					 src/fig/FIG2.h \
					 src/fig/FIGCarousel.cpp \
					 src/fig/FIGCarousel.h \
					 src/fig/TransitionHandler.h \
					 src/mpeg.h \
					 src/mpeg.c \
					 src/PrbsGenerator.cpp \
					 src/PrbsGenerator.h \
					 src/utils.cpp \
					 src/utils.h \
					 lib/crc.h \
					 lib/crc.c \
					 lib/ClockTAI.h \
					 lib/ClockTAI.cpp \
					 lib/Globals.cpp \
					 lib/Log.h \
					 lib/Log.cpp \
					 lib/RemoteControl.cpp \
					 lib/RemoteControl.h \
					 lib/Json.h \
					 lib/Json.cpp \
					 lib/edi/STIDecoder.cpp \
					 lib/edi/STIDecoder.hpp \
					 lib/edi/STIWriter.cpp \
					 lib/edi/STIWriter.hpp \
					 lib/edi/PFT.cpp \
					 lib/edi/PFT.hpp \
					 lib/edi/common.cpp \
					 lib/edi/common.hpp \
					 lib/edi/buffer_unpack.hpp \
					 lib/edioutput/AFPacket.cpp \
					 lib/edioutput/AFPacket.h \
					 lib/edioutput/EDIConfig.h \
					 lib/edioutput/PFT.cpp \
					 lib/edioutput/PFT.h \
					 lib/edioutput/TagItems.cpp \
					 lib/edioutput/TagItems.h \
					 lib/edioutput/TagPacket.cpp \
					 lib/edioutput/TagPacket.h \
					 lib/edioutput/Transport.cpp \
					 lib/edioutput/Transport.h \
					 lib/ReedSolomon.h \
					 lib/ReedSolomon.cpp \
					 lib/Socket.h \
					 lib/Socket.cpp \
					 lib/ThreadsafeQueue.h \
					 lib/zmq.hpp \
					 $(lib_fec_sources) \
					 $(lib_charset_sources)

zmqinput_keygen_SOURCES  = src/zmqinput-keygen.c
zmqinput_keygen_LDADD    = $(ZMQ_LIBS)
zmqinput_keygen_CFLAGS   = -Wall $(GITVERSION_FLAGS) $(ZMQ_CPPFLAGS)

odr_zmq2farsync_SOURCES  = src/zmq2farsync/zmq2farsync.cpp \
						   src/dabOutput/dabOutput.h \
						   src/dabOutput/dabOutputRaw.cpp \
						   lib/Globals.cpp \
						   lib/Log.h \
						   lib/Log.cpp \
						   lib/RemoteControl.cpp \
						   lib/RemoteControl.h \
						   lib/Json.h \
						   lib/Json.cpp \
						   lib/Socket.h \
						   lib/Socket.cpp \
						   lib/zmq.hpp

odr_zmq2farsync_LDADD    = $(ZMQ_LIBS)
odr_zmq2farsync_CFLAGS   = -Wall $(ZMQ_CPPFLAGS) $(PTHREAD_CFLAGS) $(GITVERSION_FLAGS) $(INCLUDE)
odr_zmq2farsync_CXXFLAGS = -Wall $(PTHREAD_CFLAGS) $(PTHREAD_LIBS) $(ZMQ_CPPFLAGS) $(GITVERSION_FLAGS) $(INCLUDE)

man_MANS = man/odr-dabmux.1

EXTRA_DIST	= COPYING NEWS README.md INSTALL.md LICENCE AUTHORS ChangeLog TODO.md doc \
			  lib/fec/README.md lib/fec/LICENSE \
			  lib/farsync/linux lib/farsync/windows \
			  lib/charset/README



