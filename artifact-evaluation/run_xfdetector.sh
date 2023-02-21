#!/bin/bash
mkdir -p xfdetector

for target in btree rbtree hashmap_atomic; do
    #for iter in {1..3}; do
    for iter in {1..1}; do
        if [[ $target = "hashmap_atomic" ]]; then
            prefix="xfdetector-hashmap_spt-$iter"
        else
            prefix="xfdetector-${target}_spt-$iter"
        fi
        echo "$prefix"
        mkdir -p xfdetector/"$prefix"-tmp
        docker run -it \
            -v /mnt/pmem0:/mnt/pmem0 \
            -v /mnt/ramdisk:/mnt/ramdisk \
            -v "$(pwd)/xfdetector/$prefix-tmp":/tmp \
            -v "$(pwd)/xfdetector":/out \
            xfdetector \
            collect-resources \
            -o "/out/$prefix.csv" -- \
            /scripts/xfdetector-test.sh "$target 0 50000 $iter" \
            >"results/$prefix.out"
        docker system prune -f
    done
done
