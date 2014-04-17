Overview
========

*ODR-DabMux* is a fork of CRC-DabMux, which was developed by the Communications
Research Center Canada. It has been forked by the
[Opendigitalradio project](http://opendigitalradio.org).

ODR-DabMux is a *DAB (Digital Audio Broadcasting) multiplexer* compliant to
ETSI EN 300 401.

In addition to the features of CRC-DabMux, this fork contains:

- configuration file support, see doc/example.mux
- timestamping support required for SFN
- a ZeroMQ ETI output that can be used with ODR-DabMod
- a ZeroMQ dabplus input that can be used with fdk-aac-dabplus
  and toolame-dab, and support ZMQ-CURVE authentication
- supports logging to syslog
- supports ZMQ input monitoring with munin tool
- includes a Telnet Remote Control for setting/getting parameters
  (can change subchannel and component labels, and ZMQ input buffer
   parameters)

The src/ directory contains the source code of ODR-DabMux.

The doc/ directory contains the ODR-DabMux documentation, and an example
configuration file.

The lib/ directory contains source code of libraries needed to build
ODR-DabMux.

Install
=======

See the INSTALL.md file for installation instructions.

Licence
=======

See the files LICENCE and COPYING

Contact
=======

Matthias P. Braendli *matthias [at] mpb [dot] li*

Pascal Charest *pascal [dot] charest [at] crc [dot] ca*

- http://opendigitalradio.org/
- http://mmbtools.crc.ca/
- http://mpb.li/

