#!/bin/bash
DAT_FILE="plots/pmdk-1dot8.dat"
cp "$DAT_FILE".template "$DAT_FILE"

printf "Tool\t\tBtree\t\t\tRbtree\t\t\tBtree (SPT)\t\tRbtree (SPT)\n\t\tCPU\tRAM\tPM\tCPU\tRAM\tPM\tCPU\tRAM\tPM\tCPU\tRAM\tPM\n"
for tool in mumak_pmdk1dot8 pmdebugger witcher; do
    printf "%s\t" $tool
    [[ $tool == "witcher" ]] && printf "\t"
    for target in btree rbtree btree_spt rbtree_spt; do
        if [[ $tool == "witcher" && $target != *"spt" ]]; then
            printf -- "- \t- \t- \t"
            continue
        else
            tail -n 1 -q results/$tool-$target-*.out | awk -F , '{time+=$1;pm+=$2;cpu+=$5;ram+=$8} END {print time/NR,cpu/NR,ram/NR,pm/NR }' >"results/$tool-$target".out
            awk '{ printf $2"\t"$3"\t"$4"\t" }' "results/$tool-$target".out
            point="${tool}_${target}_"
            value=$(cut -d " " -f 1 "results/$tool-$target".out)
            sed -i "s/\$${point^^}\$/$value/" $DAT_FILE
        fi
    done
    printf "\n"
done
printf "\n"

(
    cd plots \
        && gnuplot -c plot-comparison-pmdk-1dot8.gp &>/dev/null
)
