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
- Configuration file, see doc/example.mux and doc/example.json
- Timestamping support required for SFN
- Logging to syslog
- Monitoring using munin tool
- Includes a Telnet and ZMQ Remote Control for setting/getting parameters
- EDI input and output, both over UDP and TCP
- Support for FarSync TE1 and TE1e cards (G.703)
- Experimental STI-D(PI, X)/RTP input intended to be compatible
  with compliant encoders.
- ZeroMQ and TCP ETI outputs that can be used with ODR-DabMod
- ZeroMQ input that can be used with ODR-AudioEnc
  which supports CURVE authentication

Additional tools:

`odr-zmq2farsync`, a tool that can drive a FarSync card from a ZeroMQ ETI stream.

The `src/` directory contains the source code of ODR-DabMux and the additional
tools.

The `doc/` directory contains the ODR-DabMux documentation, a few example
configuration files, and the munin and xymon scripts for the statistics server.

The `lib/` directory contains source code of libraries needed to build
ODR-DabMux.

Up to v4.5, this repository also contained
`odr-zmq2edi`, a tool that can convert a ZeroMQ ETI stream to an EDI or ZMQ stream.
This was superseded by `digris-zmq-converter` in the
[digris-edi-zmq-bridge](https://github.com/digris/digris-edi-zmq-bridge) repository.

Install
=======

See `INSTALL.md` file for installation instructions.

You may find [ODR-DabMux-GUI](https://github.com/Opendigitalradio/ODR-DabMux-GUI/) for configuring a DAB Ensemble.

Licence
=======

See the files `LICENCE` and `COPYING`

Contributions and Contact
=========================

Contributions to this tool are welcome, you can reach users and developers
through the
[ODR-mmbTools group](https://groups.io/g/odr-mmbtools)
or any other channels mentioned on the ODR website.

There is a list of ideas and thoughts about new possible features and improvements
in the `TODO.md` file.

Developed by:

Matthias P. Braendli *matthias [at] mpb [dot] li*

Pascal Charest *pascal [dot] charest [at] crc [dot] ca*

Acknowledgements
================

David Lutton, Yoann Queret, Stefan Pöschel and Maik for bug-fix patches,
Wim Nelis for the Xymon monitoring scripts,
and many more for feedback and bug reports.

- [http://opendigitalradio.org/](http://opendigitalradio.org/)

