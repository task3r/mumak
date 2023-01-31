#!/bin/bash

readonly PM_FOLDER=/mnt/dbpmemfs

cd ~/PMDebugger/pmdk/src/examples/libpmemobj/map || exit 1
rm $PM_FOLDER/* -f
export PMEM_MMAP_HINT=0x0000100000000000
export MALLOC_MMAP_THRESHOLD_=0

if [[ $# -ne 4 ]]; then
    echo "./pmdebugger-test.sh application is_spt workload fileDate"
    exit 1
fi

#Prepare names for the measures files
application=$1
is_spt=$2
workload=$3
fileDate=$4
if [ $is_spt -eq 0 ]; then
    data_store="data_store"
    workloadOutputName="/out/pmdebugger-$application-$workload-output-$fileDate"
else
    data_store="data_store_spt"
    workloadOutputName="/out/pmdebugger-$application-spt-$workload-output-$fileDate"
fi

if [[ $application == "btree" ]] || [[ $application == "rbtree" ]]; then
    valgrind --tool=pmdebugger --print-debug-detail=yes --epoch-durability-fence=yes --redundant-logging=yes ./$data_store $application "$PM_FOLDER/testfile" $workload >>$workloadOutputName 2>&1
else
    valgrind --tool=pmdebugger --print-debug-detail=yes ./$data_store $application "$PM_FOLDER/testfile" $workload >>$workloadOutputName 2>&1
fi
