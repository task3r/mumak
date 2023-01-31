#!/bin/bash
(
    cd "$(dirname "$0")"/.. || exit 1
    docker build -t mumak:1.12.1 . --build-arg PMDK_VERSION=tags/1.12.1
)

mkdir -p minimal_example

for target in btree rbtree; do
    prefix="mumak-minimal_example-$target"
    echo "$prefix"
    docker run -it \
        -v /mnt/pmem0:/mnt/pmem0 \
        -v /mnt/ramdisk:/mnt/ramdisk \
        -v "$(pwd)"/minimal_example:/out \
        mumak:1.12.1 \
        /scripts/collect-resources.sh \
        -o "/out/$prefix.csv" -- \
        ./mumak \
        -f "/root/mumak/configs/${target}_1000.cfg" \
        -t vanilla,bfi,dofta
    docker system prune -f
done
