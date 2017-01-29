# Copyright (C) 2008, 2009 Her Majesty the Queen in Right of Canada
# (Communications Research Center Canada)

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

AC_PREREQ(2.61)
AC_INIT([ODR-DabMux], [1.2.1], [matthias.braendli@mpb.li])
AC_CONFIG_AUX_DIR([build-aux])
AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE([-Wall subdir-objects foreign])
AC_CONFIG_SRCDIR([src/DabMux.cpp])
AC_CONFIG_HEADER([config.h])
AM_SILENT_RULES([yes])

AC_CONFIG_MACRO_DIR([m4])

case $host in
*linux*)
    case $target_cpu in
    x86_64)
        AC_DEFINE([__64BIT__], [], [__16BIT__, __32BIT__])
        ;;
    *)
        AC_DEFINE([__32BIT__], [], [__16BIT__, __64BIT__])
        AC_SUBST([BUILD_TARGET], ["CC='gcc -m32' CXX='g++ -m32' --target=i686"])
        flags="-m32"
    esac
    ;;
*)
    AC_DEFINE([__32BIT__], [], [__16BIT__, __64BIT__])
    AC_SUBST([GETOPT], ["xgetopt.cpp xgetopt.h"])
    AC_SUBST([WSOCK32], ["-lwsock32"])
    AC_SUBST([BUILD_HOST], ["--host i386-mingw32msvc"])
esac

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL

AX_CXX_COMPILE_STDCXX_11(noext,mandatory)

# Checks for libraries.
AC_CHECK_LIB([pthread], [pthread_create], [], AC_MSG_ERROR([libpthread is required]))
AX_BOOST_BASE([1.48.0], [], AC_MSG_ERROR([BOOST 1.48 or later is required]))

AC_CHECK_LIB([m], [sin])

# Checks for header files.
AC_MSG_CHECKING([for OS type])
AC_PREPROC_IFELSE(
        [AC_LANG_SOURCE([
#ifndef _WIN32
#error windows
#endif
            ])],
        [OS=windows],
        [OS=linux]
        )
AC_MSG_RESULT([$OS])
AM_CONDITIONAL([WINDOWS], [test "x$OS" == "xwindows"])
AC_SUBST([FARSYNC_DIR], ['$(top_srcdir)/lib/farsync/'$OS])

AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([arpa/inet.h fcntl.h limits.h netdb.h netinet/in.h stddef.h stdint.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_HEADER_TIME
AC_STRUCT_TM
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T
AC_C_VOLATILE

# Checks for library functions.
AC_PROG_GCC_TRADITIONAL
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_REALLOC
AC_TYPE_SIGNAL
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([bzero gethostbyname gettimeofday inet_ntoa memchr memmove memset socket strchr strdup strerror strrchr strstr strtol strtoul])

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

# EDI
AC_ARG_ENABLE([output_edi],
        [AS_HELP_STRING([--disable-output-edi], [Disable EDI output])],
        [], [enable_output_edi=yes])
AS_IF([test "x$enable_output_edi" = "xyes"],
       AC_CHECK_LIB(curl, curl_easy_init, [true], [AC_MSG_ERROR([cURL is required for EDI output])]))
AS_IF([test "x$enable_output_edi" = "xyes"],
        [AC_DEFINE(HAVE_OUTPUT_EDI, [1], [Define if EDI output is enabled])])
AS_IF([test "x$enable_output_edi" = "xyes"],
        [AC_DEFINE(HAVE_CURL, [1], [Define if cURL is available])])

# ZeroMQ
AC_CHECK_LIB(zmq, zmq_init, [] , AC_MSG_ERROR(ZeroMQ libzmq is required))

AC_DEFINE([HAVE_INPUT_ZEROMQ], [1], [Define if ZeroMQ input is enabled])
AC_DEFINE([HAVE_OUTPUT_ZEROMQ], [1], [Define if ZeroMQ output is enabled])
AC_DEFINE([HAVE_RC_ZEROMQ], [1], [Define if ZeroMQ enabled for rc])

# Link against cURL
AM_CONDITIONAL([HAVE_CURL_TEST],
			   [test "x$enable_output_edi" = "xyes"])
# Do not build odr-zmq2farsync if no RAW output
AM_CONDITIONAL([HAVE_OUTPUT_RAW_TEST],
			   [test "x$enable_output_raw" = "xyes"])

AM_CONDITIONAL([IS_GIT_REPO], [test -d '.git'])

AC_CONFIG_FILES([Makefile
                 src/Makefile
                 lib/Makefile
                 lib/farsync/Makefile
                 ])
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
for output in file fifo udp tcp raw simul edi
do
    eval var=\$enable_output_$output
    AS_IF([test "x$var" = "xyes"],
          [enabled="$enabled $output"],
          [disabled="$disabled $output"])
done
echo "  Enabled: $enabled zmq"
echo "  Disabled: $disabled"

echo
echo "***********************************************"
echo
