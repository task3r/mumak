#!/bin/bash

apt update
apt install -y \
    build-essential wget curl gcc g++ gdb git tree bear unzip \
    python3 python3-pip python3-venv \
    autoconf automake pkg-config libglib2.0-dev libfabric-dev pandoc \
    libncurses5-dev libndctl-dev libdaxctl-dev

PIN_URL="https://software.intel.com/sites/landingpage/pintool/downloads/pin-3.21-98484-ge7cd811fd-gcc-linux.tar.gz"
if [ ! -d "pin" ]; then
    echo "Downloading and unpacking pin..."
    curl -sOL ${PIN_URL}
    mkdir pin
    tar xf ./*.tar.gz -C pin --strip-components 1
    rm ./*.tar.gz
    chown -R vagrant:vagrant pin
fi

if [ ! -d "pmdk" ]; then
    git clone https://github.com/pmem/pmdk.git
    (
        cd pmdk || exit
        git checkout tags/1.8
        make EXTRA_CFLAGS="-Wno-error" -j"$(nproc)"
        make install
    )
fi

mkdir ~/.ssh
chmod 700 ~/.ssh
ssh-keyscan -H github.com >>~/.ssh/known_hosts
