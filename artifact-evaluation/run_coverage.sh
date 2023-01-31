#!/bin/bash
mkdir -p coverage

for target in btree rbtree hashmap; do
    for workload_size in 1000 2000 5000 10000 25000 50000 100000; do
        for inst in all store; do
            prefix="coverage-$inst-$target-$workload_size"
            echo "$prefix"
            docker run -it \
                -v /mnt/pmem0:/mnt/pmem0 \
                -v /mnt/ramdisk:/mnt/ramdisk \
                -v "$(pwd)"/coverage:/out \
                mumak:1.6 \
                /scripts/collect-resources.sh \
                -o "/out/$prefix.csv" -- \
                ./mumak \
                -f "/root/mumak/configs/${target}_${workload_size}.cfg" \
                -t coverage \
                -a 10000000 \
                -i "$inst" >"$prefix.out"
        done
        docker system prune -f
    done
done
