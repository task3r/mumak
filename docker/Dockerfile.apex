ARG BASE="vardalith"
FROM ${BASE}

RUN apt update && apt install -y autoconf automake build-essential cmake libnuma-dev libtbb-dev python3-pip
RUN python3 -m pip install gdown

WORKDIR /root
RUN git clone https://github.com/baotonglu/apex
RUN gdown https://drive.google.com/uc?id=1Q89-v4FJLEwIKL3YY3oCeOEs0VUuv5bD

WORKDIR /root/apex
COPY apex.patch .
RUN git apply --ignore-space-change --ignore-whitespace apex.patch
RUN ./build.sh debug

WORKDIR /scripts
