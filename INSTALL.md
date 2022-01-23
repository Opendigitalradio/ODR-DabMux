Required dependencies:
======================

* A C++11 compiler
* Boost 1.48 or later
* ZeroMQ 4 or later
* (optional) cURL to download the TAI-UTC bulletin, needed for timestamps in EDI and ZMQ output.

Dependencies on Debian (and Ubuntu)
-----------------------------------

On Debian and Ubuntu you will need to install the following packages:

    sudo apt-get install build-essential libzmq5-dev automake libboost-system-dev libcurl4-openssl-dev


Dependencies on CentOS
----------------------

In addition to the packages needed to install a compiler, install the packages:
boost-devel libcurl-devel zeromq-devel

Third-party RPM packages are maintained by RaBe, and are built by the
[openSUSE Build Service](https://build.opensuse.org/project/show/home:radiorabe:dab).
For questions regarding these packages, please get in touch with the maintainer of
the [radio RaBe repository](https://github.com/radiorabe/).

For openSUSE, mnhauke is maintaining packages, also built using
[OBS](https://build.opensuse.org/project/show/home:mnhauke:ODR-mmbTools).

Compiling ODR-DabMux
====================

The *master* branch in the repository always points to the
latest release. If you are looking for a new feature or bug-fix
that did not yet make its way into a release, you can clone the
*next* branch from the repository.

* Download and install the dependencies as above
* Clone the git repository, master branch

       % git clone https://github.com/Opendigitalradio/ODR-DabMux.git

* or next branch

       % git clone -b next https://github.com/Opendigitalradio/ODR-DabMux.git

* Bootstrap autotools:

       % cd ODR-DabMux/
       % ./bootstrap.sh

* Run the configure script

       % ./configure

* Build ODR-DabMux

       % make

* Install ODR-DabMux (as root)

       % sudo make install

It is advised to run the bootstrap and configure steps again every
time you pull updates from the repository.

Develop on OSX and FreeBSD
==========================

If you want to develop on OSX platform install the necessary build tools
and dependencies with brew

    brew install boost zeromq automake curl

On FreeBSD, pkg installs all dependencies to /usr/local, but the build
tools will not search there by default. Set the following environment variables
before calling ./configure

    LDFLAGS="-L/usr/local/lib"
    CFLAGS="-I/usr/local/include"
    CXXFLAGS="-I/usr/local/include"

On both systems, RAW output is not available. Note that these systems
are not tested regularly.

Advanced installation procedure:
================================

The configure script can be launched with a variety of options, launch the
following command for a complete list:

    % ./configure --help

