#!/bin/bash
mkdir -p pmdebugger

for target in btree rbtree; do
    for is_spt in 0 1; do
        for iter in {1..3}; do
            prefix="pmdebugger-$target-$is_spt-$iter"
            echo "$prefix"
            docker run -it \
                -v /mnt/pmem0:/mnt/dbpmemfs \
                -v /mnt/ramdisk:/mnt/ramdisk \
                -v "$(pwd)"/pmdebugger:/out \
                pmdebugger \
                collect-resources \
                -o "/out/$prefix.csv" -- \
                "/scripts/pmdebugger-test.sh $target $is_spt 50000 $iter" \
                >"results/$prefix.out"
            docker system prune -f
        done
    done
done
