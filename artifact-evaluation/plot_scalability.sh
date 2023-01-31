#!/bin/bash
DAT_FILE="plots/scalability.dat"
cp "$DAT_FILE".template "$DAT_FILE"

for target in cmap stree lfhashtable hashtable redis rocksdb; do
    tail -n 1 -q results/$target-*.out | awk -F , '{time+=$1;pm+=$2;cpu+=$5;ram+=$8} END {print time/NR,cpu/NR,ram/NR,pm/NR }' >"results/$target".out
    value=$(cut -d " " -f 1 "results/$target".out)
    sed -i "s/\$${target^^}\$/$value/" $DAT_FILE
done

(
    cd plots \
        && gnuplot -c plot-scalability.gp &>/dev/null
)
