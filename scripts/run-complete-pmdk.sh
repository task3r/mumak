#!/bin/bash

for version in 1.8; do
	for tool in vanilla; do
		sleep 60
		sudo docker run -it -v /mnt/pmem0:/mnt/pmem0 -v /mnt/ramdisk:/mnt/ramdisk -v ~/results/vanilla/:/mnt/out --ulimit core=-1 mumak:$version ./mumak -t $tool -i all -x "/root/mumak/tools/pmdk/src/examples/libpmemobj/map/data_store btree /mnt/pmem0/pool 100000" -X "/root/mumak/tools/pmdk/src/examples/libpmemobj/map/data_store btree /mnt/pmem0/pool 100000" -o /mnt/out -a 100000 -r

		sleep 60
		sudo docker run -it -v /mnt/pmem0:/mnt/pmem0 -v /mnt/ramdisk:/mnt/ramdisk -v ~/results/vanilla/:/mnt/out --ulimit core=-1 mumak:$version ./mumak -t $tool -i all -x "/root/mumak/tools/pmdk/src/examples/libpmemobj/map/data_store rbtree /mnt/pmem0/pool 100000" -X "/root/mumak/tools/pmdk/src/examples/libpmemobj/map/data_store rbtree /mnt/pmem0/pool 100000" -o /mnt/out -a 100000 -r

		sleep 60
		sudo docker run -it -v /mnt/pmem0:/mnt/pmem0 -v /mnt/ramdisk:/mnt/ramdisk -v ~/results/vanilla-xf/:/mnt/out --ulimit core=-1 mumak:$version ./mumak -t $tool -i all -x "/root/mumak/tools/pmdk/src/examples/libpmemobj/map/data_store_xf btree /mnt/pmem0/pool 100000" -X "/root/mumak/tools/pmdk/src/examples/libpmemobj/map/data_store_xf btree /mnt/pmem0/pool 100000" -o /mnt/out -a 100000 -r

		sleep 60
		sudo docker run -it -v /mnt/pmem0:/mnt/pmem0 -v /mnt/ramdisk:/mnt/ramdisk -v ~/results/vanilla-xf/:/mnt/out --ulimit core=-1 mumak:$version ./mumak -t $tool -i all -x "/root/mumak/tools/pmdk/src/examples/libpmemobj/map/data_store_xf rbtree /mnt/pmem0/pool 100000" -X "/root/mumak/tools/pmdk/src/examples/libpmemobj/map/data_store_xf rbtree /mnt/pmem0/pool 100000" -o /mnt/out -a 100000 -r
		
		if [ $version = "1.6" ]; then
		sleep 60
			sudo docker run -it -v /mnt/pmem0:/mnt/pmem0 -v /mnt/ramdisk:/mnt/ramdisk -v ~/results/vanilla/:/mnt/out --ulimit core=-1 mumak:$version ./mumak -t $tool -i all -x "/root/mumak/tools/pmdk/src/examples/libpmemobj/map/data_store_hash hashmap_atomic /mnt/pmem0/pool 100000" -X "/root/mumak/tools/pmdk/src/examples/libpmemobj/map/data_store_hash hashmap_atomic /mnt/pmem0/pool 100000" -o /mnt/out -a 100000 -r
		sleep 60
			sudo docker run -it -v /mnt/pmem0:/mnt/pmem0 -v /mnt/ramdisk:/mnt/ramdisk -v ~/results/vanilla-xf/:/mnt/out --ulimit core=-1 mumak:$version ./mumak -t $tool -i all -x "/root/mumak/tools/pmdk/src/examples/libpmemobj/map/data_store_hash_xf hashmap_atomic /mnt/pmem0/pool 100000" -X "/root/mumak/tools/pmdk/src/examples/libpmemobj/map/data_store_hash_xf hashmap_atomic /mnt/pmem0/pool 100000" -o /mnt/out -a 100000 -r
		fi
	done
done
