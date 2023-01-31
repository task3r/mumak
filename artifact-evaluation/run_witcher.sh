#!/bin/bash
mkdir -p witcher

for target in btree rbtree; do
    #for iter in {1..3}; do
    for iter in {1..1}; do
        prefix="witcher-${target}_spt-$iter"
        echo "$prefix"
        mkdir -p witcher/"$prefix"-tmp
        docker run -it \
            -v /mnt/pmem0:/mnt/pmem0 \
            -v /mnt/ramdisk:/mnt/ramdisk \
            -v "$(pwd)/witcher/$prefix-tmp":/tmp \
            -v "$(pwd)"/witcher:/out \
            -m 200g --cpuset-cpus 0-120 \
            witcher \
            collect-resources \
            -o "/out/$prefix.csv" -- \
            timeout 12h bash -c \
            "/root/witcher/script/run.py $target /root/witcher/benchmark/pmdk_examples/$target/random/2000/" \
            >"results/$prefix.out"
        docker system prune -f
    done
done
