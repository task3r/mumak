ARG BASE_OS="ubuntu:18.04"
FROM ${BASE_OS}

SHELL ["/bin/bash", "-c"] 

ENV DEBIAN_FRONTEND noninteractive

RUN apt update && apt install -y \
    apt-utils build-essential man wget git htop dstat doxygen \
    gcc g++ libevent-dev libfabric-dev libseccomp-dev autoconf automake \
    pkg-config libglib2.0-dev pandoc libncurses5-dev libboost-all-dev \
    asciidoc asciidoctor bash-completion xmlto libtool libglib2.0-0 libfabric1 \
    libfabric-dev graphviz libncurses5 libkmod2 libkmod-dev libudev-dev \
    uuid-dev libjson-c-dev libkeyutils-dev time

# Install ndctl and daxctl
RUN mkdir /downloads 
#&& chmod +x /downloads 
RUN git clone https://github.com/pmem/ndctl /downloads/ndctl
WORKDIR /downloads/ndctl
RUN git checkout tags/v71.1
RUN ./autogen.sh && \
    apt install --reinstall -y systemd && \
    ./configure CFLAGS='-g -O2' --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib  && \
    make -j$(nproc) && \
    make install

WORKDIR /root
RUN git clone https://github.com/PASAUCMerced/PMDebugger.git
WORKDIR /root/PMDebugger/valgrind-pmdebugger
RUN ./autogen.sh
RUN ./configure
#Remove need for script that was not added to the project
RUN sed -i "s/filter_stderr //g" memcheck/tests/Makefile
RUN sed -i "s/mpi /#mpi/g" Makefile
RUN make -j$(nproc) && \
    make check && \
    make install
WORKDIR /root/PMDebugger/pmdk
# Replace drivers
COPY /drivers/* src/examples/libpmemobj/map/
#Remove need to install pmempool given that there is no bash_completion folder in the pmempool folder
RUN sed -i "s/pmempool //g" src/tools/Makefile
RUN make -j$(nproc) && \
    make install

COPY scripts /scripts
WORKDIR /scripts
COPY /scripts/collect-resources.sh /usr/bin/collect-resources
RUN chmod u+x -R /scripts
