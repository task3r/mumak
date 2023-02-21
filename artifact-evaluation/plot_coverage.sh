#!/bin/bash
for inst in store all; do
    file="plots/coverage-$inst.dat"
    echo "#btree,rbtree,hashmap" >$file
    for size in 1000 2000 5000 10000 25000 50000 100000; do
        printf "%s," $size >>$file
        for target in btree rbtree hashmap; do
            fp=$(grep "Failure points" "coverage-$inst-$target-$size.out" | cut -d ":" -f2 | tr -d " " | sed 's/\r$//')
            printf "%s," "$fp" >>$file
        done
        printf "\n" >>$file
    done
done

(
    cd plots \
        && gnuplot -c plot-coverage.gp &>/dev/null
)
