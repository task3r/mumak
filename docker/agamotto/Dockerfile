ARG BASE_OS="ubuntu:18.04"
FROM ${BASE_OS}

SHELL ["/bin/bash", "-c"] 

ENV DEBIAN_FRONTEND noninteractive

#1.Install dependencies
RUN apt update && apt install -y \
    apt-utils nano build-essential man wget git htop dstat valgrind gdb \
    gcc g++ autoconf automake pkg-config libglib2.0-dev libfabric-dev pandoc \
    libncurses5-dev gcc-multilib g++-multilib curl libcap-dev cmake \
    python3 python3-pip python-minimal python-pip unzip python perl minisat \
    libselinux1-dev clang-8 llvm-8 llvm-8-dev llvm-8-tools bison flex \
    libtcmalloc-minimal4 libgoogle-perftools-dev libsqlite3-dev doxygen \
    libboost-all-dev time

RUN pip3 install wllvm tabulate lit

#2.Install stp
WORKDIR /root
RUN git clone https://github.com/stp/stp
WORKDIR /root/stp
RUN git submodule init && git submodule update
RUN git checkout tags/2.3.3
RUN mkdir build
WORKDIR /root/stp/build
RUN cmake ..
RUN cmake --build .
RUN cmake --install .

#3.Initialize agamotto
WORKDIR /root
RUN git clone https://github.com/efeslab/agamotto.git -b artifact-eval-osdi20
WORKDIR  /root/agamotto
RUN git submodule init && git submodule update
RUN mkdir -p build

#4.Build klee-uclibc
WORKDIR /root/agamotto/klee-uclibc
RUN ./configure --make-llvm-lib --with-llvm-config=$(which llvm-config-8)
RUN make -j$(nproc)

#5.Build AGAMOTTO
# Replace drivers
WORKDIR /root/agamotto
COPY drivers/custom /root/agamotto/nvmbugs/custom
COPY drivers/CMakeLists.txt /root/agamotto/nvmbugs/
ARG PMDK_VERSION
RUN test -n "$PMDK_VERSION"
RUN sed -i "s|PMDK_VERSION|${PMDK_VERSION}|g" /root/agamotto/nvmbugs/CMakeLists.txt 
RUN LLVM_VERSION=8 SANITIZER_BUILD= BASE=$(realpath ./build/) REQUIRES_RTTI=1 \
    DISABLE_ASSERTIONS=1 ENABLE_DEBUG=0 ENABLE_OPTIMIZED=1 \
    ./scripts/build/build.sh libcxx && \
    source build.env && \
    cd build && \
    cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DENABLE_CRC32_SUPPORT=ON \
    -DENABLE_SOLVER_STP=ON \
    -DENABLE_POSIX_RUNTIME=ON \
    -DENABLE_KLEE_UCLIBC=ON \
    -DKLEE_UCLIBC_PATH=$(realpath ../klee-uclibc) \
    -DENABLE_UNIT_TESTS=OFF \
    -DLLVM_CONFIG_BINARY=$(which llvm-config-8) \
    -DLLVMCC=$(which clang-8) \
    -DLLVMCXX=$(which clang++-8) \
    -DCMAKE_C_COMPILER=$(which wllvm) \
    -DCMAKE_CXX_COMPILER=$(which wllvm++) \
    -DENABLE_KLEE_LIBCXX=ON \
    -DKLEE_LIBCXX_DIR=$(realpath .)/libc++-install-8/ \
    -DKLEE_LIBCXX_INCLUDE_DIR=$(realpath .)/libc++-install-8/include/c++/v1/ \
    .. && \
    make -j$(nproc)

COPY scripts /scripts
RUN chmod u+x -R /scripts
COPY /scripts/collect-resources.sh /usr/bin/collect-resources
WORKDIR /scripts
