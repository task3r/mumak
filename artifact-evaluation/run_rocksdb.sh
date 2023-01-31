#!/bin/bash
mkdir -p rocksdb

for iter in {1..3}; do
    prefix="rocksdb-$iter"
    echo "$prefix"
    docker run -it \
        -v /mnt/pmem0:/mnt/pmem0 \
        -v /mnt/ramdisk:/mnt/ramdisk \
        -v "$(pwd)/rocksdb":/out \
        rocksdb:mumak \
        /scripts/collect-resources.sh \
        -o "/out/$prefix.csv" -- \
        ./mumak \
        -f /root/mumak/configs/rocks_50000.cfg \
        -a 1000000 \
        -i allrmw \
        -t bfi,dofta >"results/$prefix.out"
    docker system prune -f
done
