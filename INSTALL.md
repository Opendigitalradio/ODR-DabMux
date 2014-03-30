Required dependencies:
======================

* libfec from Phil Karn, with compatibility patch:
[ka9q-fec](https://github.com/Opendigitalradio/ka9q-fec)
* Boost 1.48 or later
* Optional ZeroMQ 4 from [http://www.zeromq.org](http://www.zeromq.org)

Use the --disable-output-zeromq ./configure option if you don't have ZeroMQ.

Simple install procedure using tarball release:
===============================================

Install libfec
--------------

    % git clone https://github.com/Opendigitalradio/ka9q-fec.git
    % cd ka9q-fec
    % ./configure                           # Run the configure script
    % make                                  # Build the library
    [as root]
    % make install                          # Install the library

Install zeromq 4.0.3
--------------------

    % wget http://download.zeromq.org/zeromq-4.0.3.tar.gz
    % tar -f zeromq-4.0.3.tar.gz -x
    % cd zeromq-4.0.3
    % ./configure
    % make
    [as root]
    % make install

Install odr-dabmux
------------------

    % tar xjf odr-dabmux-x.y.z.tar.bz2      # Unpack the source
    % cd odr-dabmux-x.y.z                   # Change to the source directory
    % ./configure --enable-input-zeromq --enable-output-zeromq
                                            # Run the configure script
    % make                                  # Build ODR-DabMux
    [ as root ]
    % make install                          # Install ODR-DabMux

Nearly as simple install procedure using repository:
====================================================

The code in the repository is more recent than the latest
release and could be less stable, but already have new
features.

* Download and install fec as above
* Clone the git repository
* Bootstrap autotools: <pre>% ./bootstrap.sh</pre>
* Then use ./configure as above


Advanced install procedure:
===========================

The configure script can be launch with a variety of options, launch the
following command for a complete list:

    % ./configure --help

Notes about libfec
==================
The original libfec version from
[ka9q.net](http://www.ka9q.net/code/fec/fec-3.0.1.tar.bz2)
does not compile on x86\_64 nor on ARM. That is the reason why the patched
version is suggested.

On x86 systems, the original version can also be used.
