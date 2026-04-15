#!/bin/bash
mkdir -p montage
for iter in {1..3}; do
    for engine_id in 11 24; do
        [[ $engine_id -eq 11 ]] && prefix="lfhashtable-$iter"
        [[ $engine_id -eq 24 ]] && prefix="hashtable-$iter"
        echo "$prefix"
        docker run -it \
            -v /mnt/pmem0:/mnt/pmem \
            -v /mnt/ramdisk:/mnt/ramdisk \
            -v "$(pwd)/montage":/out \
            montage:mumak \
            /scripts/collect-resources.sh \
            -o "/out/$prefix.csv" -- \
            /scripts/mumak \
            -f /configs/montage_${engine_id}_50000.cfg \
            -a 1000000 \
            -i all \
            -t bfi,dofta >"results/$prefix.out"
        docker system prune -f
    done
done
