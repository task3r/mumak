ARG BASE_OS="ubuntu:18.04"
FROM ${BASE_OS}

SHELL ["/bin/bash", "-c"] 

ENV DEBIAN_FRONTEND noninteractive

RUN apt update && apt install -y \
    apt-utils build-essential man wget git htop dstat gcc g++ autoconf automake \
    pkg-config libglib2.0-dev libfabric-dev libncurses5-dev time \
    asciidoc asciidoctor bash-completion xmlto libtool pkg-config libglib2.0-0 \
    libglib2.0-dev libfabric1 doxygen graphviz libncurses5 libkmod2 libkmod-dev \
    libudev-dev uuid-dev libjson-c-dev libkeyutils-dev libboost-all-dev libevent-dev

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

#Install XFDetector
WORKDIR /root
RUN git clone https://github.com/sihangliu/xfdetector.git
WORKDIR /root/xfdetector
#Use the git checkout to obtain the version discussed in the XFDetector paper
RUN git checkout 15f7efc
#Ensure we can test the PMDK applications with bigger workloads
RUN sed -i "s/#define MAX_INSERTS 500/#define MAX_INSERTS 1000000/g" driver/data_store.c
RUN sed -i "s/PMEMOBJ_MIN_POOL/PMEMOBJ_MIN_POOL*100/g" driver/data_store.c
RUN sed -i "s/#define MAX_INSERTS 500/#define MAX_INSERTS 1000000/g" driver/data_store_hash.c
RUN sed -i "s/PMEMOBJ_MIN_POOL/PMEMOBJ_MIN_POOL*100/g" driver/data_store_hash.c
RUN sed -i "s/MAX_TIMEOUT=600/MAX_TIMEOUT=172800/g" xfdetector/run.sh
RUN sed -i "s/\${WORKLOAD}_\${TESTSIZE}/\/tmp\/\${WORKLOAD}_\${TESTSIZE}/g"  xfdetector/run.sh

#Use the necessary environment variables
ENV PIN_ROOT=/root/xfdetector/pin-3.10
ENV PATH=$PATH:$PIN_ROOT
ENV PMEM_MMAP_HINT=0x10000000000
#Compile each one of the XFDetector modules
WORKDIR /root/xfdetector/xfdetector
RUN make
WORKDIR /root/xfdetector/pmdk
RUN make EXTRA_CFLAGS="-Wno-error" -j$(nproc) && \
    make install
# drivers with uncommented removal of nodes
WORKDIR /root/xfdetector/driver
COPY drivers/* /root/xfdetector/driver/
RUN make

COPY scripts /scripts
WORKDIR /scripts
COPY /scripts/collect-resources.sh /usr/bin/collect-resources
RUN chmod u+x -R /scripts
