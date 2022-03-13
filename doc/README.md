Description
===========
ODR-DabMux is a software multiplexer that generates an ETI stream from audio
and data streams. Because of its software based architecture, many typical DAB
services can be generated and multiplexed on a single PC platform with live or
pre-recorded sources.

A DAB multiplex configuration is composed of one ensemble. An ensemble is the
entity that receivers tune to and process. An ensemble contains several
services. A service is the listener-selectable output. Each service contains
one mandatory service component which is called primary component. An audio
primary component define a program service while a data primary component
define a data service. Service can contain additional components which are
called secondary components. Maximum total number of components is 12 for
program services and 11 for data services. A service component is a link to one
subchannel (or Fast Information Data Channel). A subchannel is the physical
space used within the common interleaved frame.

                  __________________________________________________
     ENSEMBLE    |                   ODR-Ensemble                   |
                 |__________________________________________________|
                         |                 |                 |
                         |                 |                 |
                  _______V______    _______V______    _______V______
     SERVICES    | ODR-Service1 |  | ODR-Service2 |  | ODR-Service3 |
                 |______________|  |______________|  |______________|
                    |        |        |        | |______         |
                    |        |        |        |        |        |
                  __V__    __V__    __V__    __V__    __V__    __V__
     SERVICE     | SC1 |  | SC2 |  | SC3 |  | SC4 |  | SC5 |  | SC6 |
     COMPONENTS  |_____|  |_____|  |_____|  |_____|  |_____|  |_____|
                    |        |   _____|        |        |    ____|
                    |        |  |              |        |   |
     __________   __V________V__V______________V________V___V_______
    | MCI | SI | | SubCh1 | SubCh9 |  ...  | SubCh3 | SubCh60 | ... |
    |_____|____| |________|________|_______|________|_________|_____|
    Fast Information Ch.                 Main Service Channel
                      COMMON   INTERLEAVED   FRAME

Files in this folder
====================
The configuration is given in a file, this folder contains examples.

A basic example is in the file *example.mux*, a more complete view of the
settings is available in *advanced.mux* and configuration settings related to
service linking are shown in *servicelinking.mux*

An explanation on how to use the remote control is in *remote_control.txt*, and
*zmq_remote.py* illustrates how to control ODR-DabMux using the ZMQ remote
control interface.

Two scripts are used for monitoring systems: *stats_dabmux_munin.py* for Munin,
and *retodrs.pl* for Xymon. You can use *show_dabmux_stats.py* to print the
statistics to console. The values are described in *STATS.md*

*DabMux.1* is an old manpage that describes the command line options that
existed in past versions. It is kept for archive.

