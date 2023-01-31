#!/bin/bash

mkdir -p agamotto
for target in btree rbtree hashmap_atomic btree_spt rbtree_spt hashmap_atomic_spt; do
    for iter in {1..3}; do
        prefix="agamotto-$target-$iter"
        echo "$prefix"
        docker run -it \
            -v /mnt/pmem0:/mnt/pmem0 \
            -v /mnt/ramdisk:/mnt/ramdisk \
            -v "$(pwd)"/agamotto:/out \
            agamotto \
            collect-resources \
            -o "/out/$prefix.csv" -- \
            /scripts/agamotto-full-test.sh $target \
            >"results/$prefix.out"
        docker system prune -f
    done
done
