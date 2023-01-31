#!/bin/bash

for num_ops in 50000 100000; do
	alloc=$((num_ops*5))
	sudo docker run -it -v /mnt/nvram0:/mnt/pmem0 -v /mnt/ramdisk:/mnt/ramdisk -v ~/results/mumak-1.8-22/redis-benchmark/$num_ops:/mnt/test-results redis:mumak1.8 ./mumak -o /mnt/test-results -x "/root/redis/src/redis-server /root/redis/redis.conf" -X "/root/redis/src/redis-server /root/redis/redis.conf" -c "/root/redis/src/redis-benchmark -c 1 -n $num_ops -t set,get" -C "/root/redis/src/redis-benchmark -c 1 -n $num_ops -t get" -s 12 -T 10 -t bfi -r -k /scripts/kill-redis.sh -a $alloc -i all
	sudo docker run -it -v /mnt/nvram0:/mnt/pmem0 -v /mnt/ramdisk:/mnt/ramdisk -v ~/results/mumak-1.8-22/redis-benchmark/$num_ops:/mnt/test-results redis:mumak1.8 ./mumak -o /mnt/test-results -x "/root/redis/src/redis-server /root/redis/redis.conf" -c "/root/redis/src/redis-benchmark -c 1 -n $num_ops -t set,get" -s 12 -t onta -r -k /scripts/kill-redis.sh

	sudo docker run -it -v /mnt/nvram0:/mnt/pmem0 -v /mnt/ramdisk:/mnt/ramdisk -v ~/results/mumak-1.8-22/redis-client/$num_ops:/mnt/test-results redis:mumak1.8 ./mumak -o /mnt/test-results -x "/root/redis/src/redis-server /root/redis/redis.conf" -X "/root/redis/src/redis-server /root/redis/redis.conf" -c "/root/redis-client/redis-client.exe $num_ops" -C "/root/redis-client/redis-client.exe $num_ops" -s 12 -T 10 -t bfi -r -k /scripts/kill-redis.sh -a $alloc -i all
	sudo docker run -it -v /mnt/nvram0:/mnt/pmem0 -v /mnt/ramdisk:/mnt/ramdisk -v ~/results/mumak-1.8-22/redis-client/$num_ops:/mnt/test-results redis:mumak1.8 ./mumak -o /mnt/test-results -x "/root/redis/src/redis-server /root/redis/redis.conf" -c "/root/redis-client/redis-client.exe $num_ops" -s 12 -t onta -r -k /scripts/kill-redis.sh
done

