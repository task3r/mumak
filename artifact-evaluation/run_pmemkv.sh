#!/bin/bash
mkdir -p pmemkv

for iter in {1..3}; do
    for engine in cmap stree; do
        prefix="$engine-$iter"
        echo "$prefix"
        docker run -it \
            -v /mnt/pmem0:/mnt/pmem0 \
            -v /mnt/ramdisk:/mnt/ramdisk \
            -v "$(pwd)/pmemkv":/out \
            pmemkv:mumak \
            /scripts/collect-resources.sh \
            -o "/out/$prefix.csv" -- \
            ./mumak \
            -f /root/mumak/configs/pmemkv_${engine}_50000.cfg \
            -a 1000000 \
            -i all \
            -t bfi,dofta >"results/$prefix.out"
        docker system prune -f
    done
done
