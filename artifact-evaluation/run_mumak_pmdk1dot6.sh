#!/bin/bash
mkdir -p mumak_pmdk1dot6

for target in btree rbtree hashmap btree_spt rbtree_spt hashmap_spt; do
    for iter in {1..3}; do
        prefix="mumak_pmdk1dot6-$target-$iter"
        echo "$prefix"
        docker run -it \
            -v /mnt/pmem0:/mnt/pmem0 \
            -v /mnt/ramdisk:/mnt/ramdisk \
            -v "$(pwd)"/mumak_pmdk1dot6:/out \
            mumak:1.6 \
            /scripts/collect-resources.sh \
            -o "/out/$prefix.csv" -- \
            ./mumak \
            -f "/root/mumak/configs/${target}_50000.cfg" \
            -t bfi,dofta >"results/$prefix.out"
        docker system prune -f
    done
done
