#!/bin/bash
#You may also use the meld tool to obtain the differences for each state-of-the-art tool:
#meld toolPMDK originalPMDK

mkdir -p /tmp/get-annotations && cd "$_" || exit

function printNumLinesWithoutComments {
    # get nodes that should be omitted from diff
    for var in $1; do
        omit_args="-x $var $omit_args"
    done

    pwd
    # get added lines, omit blanks and single-line comments
    echo "diff -r --suppress-common-lines $omit_args $2"
    # don't understand why but had to pipe the command for it to recognize args
    echo "diff -r --suppress-common-lines $omit_args $2" | bash | grep '<' | grep -v '<\s*$' | grep -v '<\s*//' >total_diff
    # get start of multi-line comments
    start=$(grep -n '<.*/\*' total_diff | cut -f1 -d:)
    # get end of multi-line comments
    end=$(grep -n '<.*\*/' total_diff | cut -f1 -d:)
    comments=$(paste <(echo "$start") <(echo "$end") | awk '{l+=$2-$1} END{print l}')
    # print this just for sanity
    echo "$(wc -l <total_diff | sed 's/[[:space:]]//g') - $comments"
    echo "$(wc -l <total_diff) - $comments" | bc

    echo "diff -r --suppress-common-lines $omit_args $2" | bash | grep "Only in"
    # rm total_diff
}

#Obtain number of PMTest annotations
echo "[PMTest Annotations]"
echo "Cloning repos..."
(
    mkdir pmtest && cd "$_" || exit
    git clone --quiet https://github.com/sihangliu/pmtest.git
    git clone --quiet https://github.com/pmem/pmdk.git
    (cd pmdk && git checkout --quiet 3764e98fe7d7495a1a6403a8acca350a4aa9d860)

    echo "Applications Annotations:"
    #diff -r --suppress-common-lines -x libart -x .gitignore pmtest/nvml/src/examples/ pmdk/src/examples/ | grep "<" | wc -l
    printNumLinesWithoutComments "libart .gitignore" "pmtest/nvml/src/examples/ pmdk/src/examples/"
    echo "PMDK Modifications:"
    #diff -r --suppress-common-lines -x benchmarks -x examples -x Makefile.nvml -x heap_layout.h -x test -x Makefile pmtest/nvml/src/ pmdk/src/ | grep "<" | wc -l
    printNumLinesWithoutComments "benchmarks examples Makefile Makefile.nvml heap_layout" "pmtest/nvml/src/ pmdk/src/"
)

#Obtain number of XFDetector annotations
echo "[XFDetector Annotations]"
echo "Cloning repos..."
(
    mkdir xfdetector && cd "$_" || exit
    git clone --quiet https://github.com/sihangliu/xfdetector.git
    git clone --quiet https://github.com/pmem/pmdk.git
    (cd xfdetector && git checkout --quiet 15f7efc)
    (cd pmdk && git checkout --quiet b13d1e2a3e1e0e444fcfba054dd55215403908da)

    echo "Applications Annotations:"
    #diff -r --suppress-common-lines xfdetector/pmdk/src/examples/ pmdk/src/examples/ | grep "<" | wc -l
    printNumLinesWithoutComments "" "xfdetector/pmdk/src/examples/ pmdk/src/examples/"
    echo "PMDK Modifications:"
    #diff -r --suppress-common-lines -x benchmarks -x examples -x tx.h xfdetector/pmdk/src/ pmdk/src/ | grep "<" | wc -l
    printNumLinesWithoutComments "benchmarks examples" "xfdetector/pmdk/src/ pmdk/src/"
)

#Obtain number of PMDebugger annotations
echo "[PMDebugger Annotations]"
echo "Cloning repos..."
(
    mkdir pmdebugger && cd "$_" || exit
    git clone --quiet https://github.com/PASAUCMerced/PMDebugger.git
    git clone --quiet https://github.com/pmem/pmdk.git
    (cd pmdk && git checkout --quiet 1.8)
    echo "Applications Annotations:"
    #diff -r --suppress-common-lines -x Makefile PMDebugger/pmdk/src/examples/ pmdk/src/examples/ | grep "<" | wc -l
    printNumLinesWithoutComments "Makefile" "PMDebugger/pmdk/src/examples/ pmdk/src/examples/"
    echo "PMDK Modifications:"
    #diff -r --suppress-common-lines -x benchmarks -x examples -x obj.c -x README.md PMDebugger/pmdk/src/ pmdk/src/ | grep "<" | wc -l
    printNumLinesWithoutComments "benchmarks examples README.md" "PMDebugger/pmdk/src/ pmdk/src/"
)
