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
     ENSEMBLE           |                   ODR-Ensemble                   |
                        |__________________________________________________|
                                |                 |                 |
                                |                 |                 |
                         _______V______    _______V______    _______V______
     SERVICES           | ODR-Service1 |  | ODR-Service2 |  | ODR-Service3 |
                        |______________|  |______________|  |______________|
                           |        |        |        | |______         |
                           |        |        |        |        |        |
                         __V__    __V__    __V__    __V__    __V__    __V__
     SERVICE            | SC1 |  | SC2 |  | SC3 |  | SC4 |  | SC5 |  | SC6 |
     COMPONENTS         |_____|  |_____|  |_____|  |_____|  |_____|  |_____|
                           |        |   _____|        |        |    ____|
                           |        |  |              |        |   |
     _________________   __V________V__V______________V________V___V_______
    | MCI | SI | FIDC | | SubCh1 | SubCh9 |  ...  | SubCh3 | SubCh60 | ... |
    |_____|____|______| |________|________|_______|________|_________|_____|
    Fast Information Ch.                 Main Service Channel
                      COMMON   INTERLEAVED   FRAME

Configuration
=============
The configuration is given in a file, please see the example in doc/example.mux

