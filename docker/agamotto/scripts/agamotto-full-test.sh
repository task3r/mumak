#!/bin/bash

if [[ $# -ne 1 ]]; then
    echo "./agamotto.sh application"
    exit 1
fi

function run {
    application=$1
    set +x
    source /root/agamotto/artifact/targets/common.sh

    time $KLEE \
        --output-dir="$OUTDIR"/"$application" \
        --search=nvm --nvm-heuristic-type=static \
        --custom-checkers=false --write-errors-only=true \
        --max-time=$((60 * 60 * 24)) --max-memory=10000 \
        --disable-verify=true --libc=uclibc \
        --link-llvm-lib="$PMDK_DIR"/libpmemobj.so.bc \
        --link-llvm-lib="$PMDK_DIR"/libpmem.so.bc \
        --posix-runtime --env-file="$PMDK_DIR"/pmdk.env \
        "$BUILD"/bin/"$application".bc \
        --sym-pmem-delay PMEM 8388608 PMEM \
        --sym-arg 2
}

#Prepare names for the measures files
application="$1"
ts=$(date +%y%m%d%H%M)
output_dir="/out/"

#Load variables from the common.sh file
set +x
source ~/agamotto/artifact/targets/common.sh

export -f run
timeout 12h bash -c "run $application"

#Report whether test timeouted
exit_status=$?
if [[ $exit_status -eq 124 ]]; then
    echo "Testing of $application did not finish"
fi

cp "$OUTDIR/$application/info" "$output_dir/info-$ts"
cp "$OUTDIR/$application/all.pmem.err" "$output_dir/all.pmem.err-$ts"
