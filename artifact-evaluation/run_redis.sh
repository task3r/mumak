#!/bin/bash
mkdir -p redis

for iter in {1..3}; do
    prefix="redis-$iter"
    echo "$prefix"
    docker run -it \
        -v /mnt/pmem0:/mnt/pmem0 \
        -v /mnt/ramdisk:/mnt/ramdisk \
        -v "$(pwd)/redis":/out \
        redis:mumak \
        /scripts/collect-resources.sh \
        -o "/out/$prefix.csv" -- \
        ./mumak \
        -f /root/mumak/configs/redis_50000.cfg \
        -a 10000000 \
        -i all \
        -t bfi,dofta >"results/$prefix.out"
    docker system prune -f
done
