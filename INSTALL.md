You have 3 ways to install odr-dabmux on your host:

# Using binary debian packages
If your host is running a debian-based OS and its cpu is one of amd64, arm64 or arm/v7, then you can install odr-dabmux using the standard debian packaging system:
1. Update the debian apt repository list:
   ```
   curl -fsSL http://debian.opendigitalradio.org/odr.asc | sudo tee /etc/apt/trusted.gpg.d/odr.asc 1>/dev/null
   curl -fsSL http://debian.opendigitalradio.org/odr.list | sudo tee /etc/apt/sources.list.d/odr.list 1>/dev/null
   ```
1. Refresh the debian packages list:
   ```
   apt update
   ```
1. Install odr-audioenc:
   ```
   sudo apt install --yes odr-dabmux
   ```

**Attention**: odr-dabmux (4.2.1-1) does not include the Mux Web Management GUI

# Using the dab-scripts
You can compile odr-dabmux as well as the other main components of the mmbTools set with an installation script:
1. Clone the dab-scripts repository:
   ```
   git clone https://github.com/opendigitalradio/dab-scripts.git
   ```
1. Follow the [instructions](https://github.com/Opendigitalradio/dab-scripts/tree/master/install)

# Compiling manually
Unlike the 2 previous options, this one allows you to compile odr-dabmux with the features you really need.

## Dependencies
### Debian Bullseye-based OS:
```
# Required packages
## C++11 compiler
sudo apt-get install --yes build-essential automake libtool

## ZeroMQ
sudo apt-get install --yes libzmq3-dev libzmq5

## Boost 1.48 or later
sudo apt-get install --yes libboost-system-dev

# optional packages
## cURL to download the TAI-UTC bulletin, needed for timestamps in EDI output
sudo apt-get install --yes libcurl4-openssl-dev
```

### Dependencies on other linux distributions
For CentOS, in addition to the packages needed to install a compiler, install the packages:
boost-devel libcurl-devel zeromq-devel

Third-party RPM packages are maintained by RaBe, and are built by the
[openSUSE Build Service](https://build.opensuse.org/project/show/home:radiorabe:dab).
For questions regarding these packages, please get in touch with the maintainer of
the [radio RaBe repository](https://github.com/radiorabe/).

For openSUSE, mnhauke is maintaining packages, also built using
[OBS](https://build.opensuse.org/project/show/home:mnhauke:ODR-mmbTools).

## Compilation
The *master* branch in the repository always points to the
latest release. If you are looking for a new feature or bug-fix
that did not yet make its way into a release, you can clone the
*next* branch from the repository.

1. Clone this repository:
   ```
   # stable version:
   git clone https://github.com/Opendigitalradio/ODR-DabMux.git

   # or development version (at your own risk):
   git clone https://github.com/Opendigitalradio/ODR-DabMux.git -b next
   ```
1. Configure the project
   ```
   cd ODR-DabMux
   ./bootstrap
   ./configure
   ```
1. Compile and install:
   ```
   make
   sudo make install
   ```

Notes:
- It is advised to run the bootstrap and configure steps again every time you pull updates from the repository.
- The configure script can be launched with a variety of options. Run `./configure --help` to display a complete list

# Develop on OSX and FreeBSD
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
