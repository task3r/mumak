ARG BASE_OS="ubuntu:18.04"
FROM ${BASE_OS}

SHELL ["/bin/bash", "-c"]

ENV DEBIAN_FRONTEND noninteractive

RUN apt update && apt install -y \
    apt-utils build-essential man wget git htop dstat procps gdb telnet time \
    gcc g++ libevent-dev libfabric-dev libseccomp-dev autoconf automake \
    pkg-config libglib2.0-dev libncurses5-dev libboost-all-dev \
    asciidoc asciidoctor bash-completion xmlto libtool libglib2.0-0 libfabric1 \
    graphviz libncurses5 libkmod2 libkmod-dev libudev-dev uuid-dev \
    libjson-c-dev libkeyutils-dev pandoc cmake libelf-dev xxd curl

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
ARG PIN_VERSION=pin-3.28-98749-g6643ecee5
RUN wget -q http://software.intel.com/sites/landingpage/pintool/downloads/${PIN_VERSION}-gcc-linux.tar.gz && \
    tar -xf ${PIN_VERSION}-gcc-linux.tar.gz && \
    rm -f ${PIN_VERSION}-gcc-linux.tar.gz

# Download and install Zydis
ARG ZYDIS_VERSION=tags/v4.0.0
RUN git clone --recursive https://github.com/zyantific/zydis.git
WORKDIR $MUMAK_ROOT/tools/zydis
RUN git checkout $ZYDIS_VERSION
WORKDIR $MUMAK_ROOT/tools/zydis/build
RUN cmake .. && make && make install
WORKDIR $MUMAK_ROOT/tools/zydis/dependencies/zycore/build
RUN cmake .. && make && make install

# Download and install cxxopts
WORKDIR $MUMAK_ROOT/tools/
RUN git clone https://github.com/jarro2783/cxxopts.git
RUN cp cxxopts/include/cxxopts.hpp /usr/include

# Download and install oneTBB
RUN apt install -y libtbb-dev
#WORKDIR $MUMAK_ROOT/tools
#RUN git clone https://github.com/oneapi-src/oneTBB
#WORKDIR $MUMAK_ROOT/tools/oneTBB
#RUN cmake . && cmake --build . && make install

# Install rust
RUN curl https://sh.rustup.rs -sSf | sh -s -- -y
ENV PATH="/root/.cargo/bin:${PATH}"

# Download PMDK
WORKDIR $MUMAK_ROOT/tools
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

WORKDIR $MUMAK_ROOT/tools
RUN git clone https://github.com/GJDuck/e9patch
WORKDIR $MUMAK_ROOT/tools/e9patch
RUN ./build.sh
ENV E9ROOT=$MUMAK_ROOT/tools/e9patch

# Build Mumak
COPY src/ $MUMAK_ROOT/src
WORKDIR $MUMAK_ROOT/src
ENV PIN_ROOT=$MUMAK_ROOT/tools/$PIN_VERSION-gcc-linux
ENV PIN=$PIN_ROOT/pin
ARG DEBUG=0
RUN make clean-all ; [[ "$DEBUG" -eq 1 ]] && make debug || make

# Add required scripts
RUN mkdir /scripts
COPY scripts/ /scripts
COPY mumak /scripts
WORKDIR /scripts
RUN chmod u+x -R /scripts
COPY configs $MUMAK_ROOT/configs

RUN ln -s $MUMAK_ROOT/tools/pmdk/src/debug/libpmem.so.1 /usr/lib/libpmem.so.1
RUN ln -s $MUMAK_ROOT/tools/pmdk/src/debug/libpmemobj.so.1 /usr/lib/libpmemobj.so.1
ENV PMEM_IS_PMEM_FORCE=1
ENV LIBPIFRRT_ROOT=$MUMAK_ROOT/src/runtime
