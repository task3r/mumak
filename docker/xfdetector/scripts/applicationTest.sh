#!/bin/bash

if [[ $# -ne 4 ]]; then
    echo "./applicationTest.sh application init_workload workload fileDate"
    echo "application = hashmap_atomic, btree or rbtree"
    exit 1
fi

cd ~/xfdetector/xfdetector

export PIN_ROOT=~/xfdetector/pin-3.10
export PATH=$PATH:$PIN_ROOT
export PMEM_MMAP_HINT=0x10000000000

#Prepare names for the measures files
application=$1
init_workload=$2
workload=$3
fileDate=$4
workloadOutputName="/out/xfdetector-$application-$workload-output-$fileDate"

#Execute the test
if [ $application = "hashmap_atomic" ]; then
    timeout -s SIGKILL 12h ./run.sh $application $init_workload $workload hash >>$workloadOutputName 2>&1
else
    timeout -s SIGKILL 12h ./run.sh $application $init_workload $workload >>$workloadOutputName 2>&1
fi
