Overview
========

ODR-DabMux is a *DAB (Digital Audio Broadcasting) multiplexer* compliant to
ETSI EN 300 401. It is the continuation of the work started by the
Communications Research Center Canada on CRC-DabMux, and is now pursued in the
[Opendigitalradio project](http://opendigitalradio.org).

ODR-DabMux is part of the ODR-mmbTools tool set. More information about the
ODR-mmbTools is available in the *guide*, available on the
[Opendigitalradio mmbTools page](http://www.opendigitalradio.org/mmbtools).

Features of ODR-DabMux:
- Standards-compliant DAB multiplexer
- Configuration file, see doc/example.mux
- Timestamping support required for SFN
- ZeroMQ and TCP ETI outputs that can be used with ODR-DabMod
- ZeroMQ input that can be used with ODR-AudioEnc
  which supports CURVE authentication
- Logging to syslog
- Monitoring using munin tool
- Includes a Telnet and ZMQ Remote Control for setting/getting parameters
- EDI output
- Support for FarSync TE1 cards (G.703)
- Something that will one day become a nice GUI for configuration,
  see gui/README.md

The src/ directory contains the source code of ODR-DabMux.

The doc/ directory contains the ODR-DabMux documentation, a few example
configuration file, and the example munin script for the statistics
server.

The lib/ directory contains source code of libraries needed to build
ODR-DabMux.

Install
=======

See the INSTALL.md file for installation instructions.

Licence
=======

See the files LICENCE and COPYING

Contributions and Contact
=========================

Contributions to this tool are welcome, you can reach users and developers
through the CRC-mmbTools google group.

There is a list of ideas and thoughts about new possible features and improvements
in the TODO file.

Developed by:

Matthias P. Braendli *matthias [at] mpb [dot] li*

Pascal Charest *pascal [dot] charest [at] crc [dot] ca*

- http://opendigitalradio.org/

