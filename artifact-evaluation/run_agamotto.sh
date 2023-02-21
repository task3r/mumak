#!/bin/bash

mkdir -p agamotto
for target in btree rbtree hashmap_atomic btree_spt rbtree_spt hashmap_atomic_spt; do
    for iter in {1..3}; do
        if [[ $target = "hashmap_atomic" ]]; then
            prefix="agamotto-hashmap-$iter"
        elif [[ $target = "hashmap_atomic_spt" ]]; then
            prefix="agamotto-hashmap_spt-$iter"
        else
            prefix="agamotto-$target-$iter"
        fi
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
