ARG BASE_OS="ubuntu:18.04"
FROM ${BASE_OS}

SHELL ["/bin/bash", "-c"] 

ENV DEBIAN_FRONTEND noninteractive

RUN apt update && apt install -y \
    apt-utils build-essential man wget git htop dstat procps gdb telnet time \
    gcc g++ libevent-dev libfabric-dev libseccomp-dev autoconf automake \
    pkg-config libglib2.0-dev libncurses5-dev libboost-all-dev \
    asciidoc asciidoctor bash-completion xmlto libtool libglib2.0-0 libfabric1 \
    doxygen graphviz libncurses5 libkmod2 libkmod-dev libudev-dev uuid-dev \
    libjson-c-dev libkeyutils-dev pandoc cmake

# Install ndctl and daxctl
RUN mkdir /downloads 
RUN git clone https://github.com/pmem/ndctl /downloads/ndctl
WORKDIR /downloads/ndctl
RUN git checkout tags/v71.1
RUN ./autogen.sh && \
    apt install --reinstall -y systemd && \
    ./configure CFLAGS='-g -O2' --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib  && \
    make -j$(nproc) && \
    make install

# TODO: change /root/mumak to /mumak for simplicity
ENV MUMAK_ROOT=/root/mumak
RUN mkdir $MUMAK_ROOT
RUN mkdir $MUMAK_ROOT/tools
WORKDIR $MUMAK_ROOT/tools

# Download PIN
ARG PIN_VERSION=pin-3.21-98484-ge7cd811fd
RUN wget -q http://software.intel.com/sites/landingpage/pintool/downloads/${PIN_VERSION}-gcc-linux.tar.gz && \
    tar -xf ${PIN_VERSION}-gcc-linux.tar.gz && \
    rm -f ${PIN_VERSION}-gcc-linux.tar.gz

# Download PMDK
RUN git clone https://github.com/pmem/pmdk.git
WORKDIR $MUMAK_ROOT/tools/pmdk
# Confirm that version was passed as an argument
ARG PMDK_VERSION
RUN test -n "$PMDK_VERSION"
ARG REFRESH
RUN git pull && git checkout ${PMDK_VERSION}
# Replace Drivers
COPY drivers/* $MUMAK_ROOT/tools/pmdk/src/examples/libpmemobj/map/
# Don't build documentation
RUN touch .skip-doc
RUN make EXTRA_CFLAGS="-Wno-error" -j$(nproc) && make install

# Build Mumak
COPY instrumentation/ $MUMAK_ROOT/instrumentation/
WORKDIR $MUMAK_ROOT/instrumentation/src
ENV PIN_ROOT=$MUMAK_ROOT/tools/pin-3.21-98484-ge7cd811fd-gcc-linux/
ARG DEBUG=0
RUN make clean && make DEBUG=${DEBUG}

# Add required scripts
RUN mkdir /scripts
COPY scripts/ /scripts
COPY mumak /scripts
WORKDIR /scripts
RUN chmod u+x -R /scripts
COPY configs $MUMAK_ROOT/configs

ENV LD_PRELOAD=$MUMAK_ROOT/tools/pmdk/src/debug/libpmem.so.1:$MUMAK_ROOT/tools/pmdk/src/debug/libpmemobj.so.1
ENV PMEM_IS_PMEM_FORCE=1
