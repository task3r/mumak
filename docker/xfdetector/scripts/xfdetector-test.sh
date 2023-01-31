#!/bin/bash

readonly PM_FOLDER=/mnt/pmem0

rm $PM_FOLDER/* -f

if [[ $# -ne 4 ]]; then
    echo "./xfdetector-test.sh application init_workload workload fileDate"
    echo "application = hashmap_atomic, btree or rbtree"
    exit 1
fi

#Prepare names for the measures files
application=$1
init_workload=$2
workload=$3
fileDate=$4

./applicationTest.sh $application $init_workload $workload $fileDate 2>&1

# Clear auxiliary file
: >/tmp/func_map
: >/root/xfdetector/xfdetector/post.out
