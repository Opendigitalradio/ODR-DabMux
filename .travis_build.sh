#!/bin/bash

pushd /tmp
git clone https://github.com/Opendigitalradio/ka9q-fec.git
cd ka9q-fec
mkdir build
cd build
cmake ..
make
sudo make install
popd

./bootstrap.sh
./configure --enable-input-prbs --enable-input-test --enable-input-udp --enable-output-raw
make
