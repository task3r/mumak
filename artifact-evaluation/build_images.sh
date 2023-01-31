#!/bin/bash

# go to the repo root dir
cd "$(dirname "$0")"/.. || exit 1

# PMDK 1.6 experiments
docker build -t mumak:1.6 . --build-arg PMDK_VERSION=tags/1.6
(
    cd docker/agamotto || exit 1
    docker build -t agamotto . --build-arg PMDK_VERSION=tags/1.6
)
(
    cd docker/xfdetector || exit 1
    docker build -t xfdetector .
)

# PMDK 1.8 experiments
docker build -t mumak:1.8 . --build-arg PMDK_VERSION=tags/1.8
(
    cd docker/witcher || exit 1
    docker build -t witcher .
)
(
    cd docker/pmdebugger || exit 1
    docker build -t pmdebugger .
)

# Scalability experiments
docker build -t mumak:1.12.1 . --build-arg PMDK_VERSION=tags/1.12.1
docker build -t mumak:1.12.1-ubuntu20 . --build-arg PMDK_VERSION=tags/1.12.1 --build-arg BASE_OS=ubuntu:20.04
docker build -t rocksdb:mumak . -f docker/Dockerfile.rocksdb
docker build -t redis:mumak . -f docker/Dockerfile.redis
docker build -t pmemkv:mumak . -f docker/Dockerfile.pmemkv
(
    cd docker/montage || exit 1
    docker build -t montage:mumak .
)
