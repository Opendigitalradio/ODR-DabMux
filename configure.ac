# Copyright (C) 2008, 2009 Her Majesty the Queen in Right of Canada
# (Communications Research Center Canada)
#
# Copyright (C) 2019 Matthias P. Braendli, http://opendigitalradio.org

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

AC_PREREQ(2.69)
AC_INIT([ODR-DabMux], [2.4.1], [matthias.braendli@mpb.li])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE([-Wall foreign subdir-objects])
AC_CONFIG_SRCDIR([src/DabMux.cpp])
AC_CONFIG_HEADER([config.h])
AM_SILENT_RULES([yes])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL

AX_CXX_COMPILE_STDCXX_11(noext,mandatory)

# Checks for libraries.
AX_PTHREAD([], AC_MSG_ERROR([requires pthread]))
AX_BOOST_BASE([1.48.0], [], AC_MSG_ERROR([BOOST 1.48 or later is required]))
AX_BOOST_SYSTEM
AX_BOOST_ASIO

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h fcntl.h limits.h netdb.h netinet/in.h stddef.h stdint.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h unistd.h])

# Options
# Outputs
# FILE
AC_ARG_ENABLE([output_file],
        [AS_HELP_STRING([--disable-output-file], [Disable FILE output])],
        [], [enable_output_file=yes])
AS_IF([test "x$enable_output_file" = "xyes"],
        [AC_DEFINE(HAVE_OUTPUT_FILE, [1], [Define if FILE output is enabled])])
# FIFO
AC_ARG_ENABLE([output_fifo],
        [AS_HELP_STRING([--disable-output-fifo], [Disable FIFO output])],
        [], [enable_output_fifo=yes])
AS_IF([test "x$enable_output_fifo" = "xyes"],
        [AC_DEFINE(HAVE_OUTPUT_FIFO, [1], [Define if FIFO output is enabled])])
# UDP
AC_ARG_ENABLE([output_udp],
        [AS_HELP_STRING([--disable-output-udp], [Disable UDP output])],
        [], [enable_output_udp=yes])
AS_IF([test "x$enable_output_udp" = "xyes"],
        [AC_DEFINE(HAVE_OUTPUT_UDP, [1], [Define if UDP output is enabled])])
# TCP
AC_ARG_ENABLE([output_tcp],
        [AS_HELP_STRING([--disable-output-tcp], [Disable TCP output])],
        [], [enable_output_tcp=yes])
AS_IF([test "x$enable_output_tcp" = "xyes"],
        [AC_DEFINE(HAVE_OUTPUT_TCP, [1], [Define if TCP output is enabled])])
# RAW
AC_ARG_ENABLE([output_raw],
        AS_HELP_STRING([--enable-output-raw], [Enable RAW output]))
AS_IF([test "x$enable_output_raw" = "xyes"],
        [AC_DEFINE(HAVE_OUTPUT_RAW, [1], [Define if RAW output is enabled])])
# SIMUL
AC_ARG_ENABLE([output_simul],
        [AS_HELP_STRING([--disable-output-simul], [Disable SIMUL output])],
        [], [enable_output_simul=yes])
AS_IF([test "x$enable_output_simul" = "xyes"],
        [AC_DEFINE(HAVE_OUTPUT_SIMUL, [1], [Define if SIMUL output is enabled])])

# EDI and ZMQ output metadata require TAI-UTC offset, which requires downloading the IETF TAI bulletin
AC_CHECK_LIB(curl, curl_easy_init)
have_curl=$ac_cv_lib_curl_curl_easy_init

AS_IF([test "x$have_curl" = "xyes"],
             [AC_DEFINE(HAVE_CURL, [1], [Define if cURL is available])])

AS_IF([test "x$have_curl" = "xno"],
             [AC_MSG_WARN([cURL not found, timestamps will not work])])

AC_LANG_PUSH([C++])
AX_CHECK_COMPILE_FLAG([-Wduplicated-cond], [CXXFLAGS="$CXXFLAGS -Wduplicated-cond"], [], ["-Werror"])
AX_CHECK_COMPILE_FLAG([-Wduplicated-branches], [CXXFLAGS="$CXXFLAGS -Wduplicated-branches"], [], ["-Werror"])
AX_CHECK_COMPILE_FLAG([-Wlogical-op], [CXXFLAGS="$CXXFLAGS -Wlogical-op"], [], ["-Werror"])
AX_CHECK_COMPILE_FLAG([-Wrestrict], [CXXFLAGS="$CXXFLAGS -Wrestrict"], [], ["-Werror"])
AX_CHECK_COMPILE_FLAG([-Wdouble-promotion], [CXXFLAGS="$CXXFLAGS -Wdouble-promotion"], [], ["-Werror"])
AX_CHECK_COMPILE_FLAG(["-Wformat=2"], [CXXFLAGS="$CXXFLAGS -Wformat=2"], [], ["-Werror"])

# Linux defines MSG_NOSIGNAL, some other systems have SO_NOSIGPIPE instead
AC_MSG_CHECKING(for MSG_NOSIGNAL)
AC_COMPILE_IFELSE([ AC_LANG_PROGRAM([[
                    #include <sys/socket.h>
                    int f = MSG_NOSIGNAL;
                    ]])],
                   [ AC_MSG_RESULT(yes)
                     AC_DEFINE(HAVE_MSG_NOSIGNAL, 1, [Define this symbol if you have MSG_NOSIGNAL]) ],
                   [ AC_MSG_RESULT(no) ])

AC_MSG_CHECKING(for SO_NOSIGPIPE)
AC_COMPILE_IFELSE([ AC_LANG_PROGRAM([[
                    #include <sys/socket.h>
                    int f = SO_NOSIGPIPE;
                    ]])],
                   [ AC_MSG_RESULT(yes)
                     AC_DEFINE(HAVE_SO_NOSIGPIPE, 1, [Define this symbol if you have SO_NOSIGPIPE]) ],
                   [ AC_MSG_RESULT(no) ])

AC_LANG_POP([C++])

# ZeroMQ
AX_ZMQ([4.0.0], [], AC_MSG_ERROR(ZeroMQ 4.0.0 is required))

AC_DEFINE([HAVE_INPUT_ZEROMQ], [1], [Define if ZeroMQ input is enabled])
AC_DEFINE([HAVE_OUTPUT_ZEROMQ], [1], [Define if ZeroMQ output is enabled])
AC_DEFINE([HAVE_RC_ZEROMQ], [1], [Define if ZeroMQ enabled for rc])

# Do not build odr-zmq2farsync if no RAW output
AM_CONDITIONAL([HAVE_OUTPUT_RAW_TEST],
			   [test "x$enable_output_raw" = "xyes"])

AM_CONDITIONAL([IS_GIT_REPO], [test -d '.git'])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

echo
echo "***********************************************"
echo
echo "ZeroMQ management server enabled."
echo
echo "Inputs: prbs udp zmq fifo file"
echo
echo "Formats: raw mpeg packet dabplus dmb epm"

echo
echo "Outputs:"
enabled=""
disabled=""
for output in file fifo udp tcp raw simul
do
    eval var=\$enable_output_$output
    AS_IF([test "x$var" = "xyes"],
          [enabled="$enabled $output"],
          [disabled="$disabled $output"])
done
echo "  Enabled: $enabled zmq"
echo "  Disabled: $disabled"

if test "$have_curl" = "no" ; then
echo
echo "WARNING! cURL not found: ODR-DabMux will not support timestamps"
echo
fi


echo
echo "***********************************************"
echo
