#!/bin/bash

if [[ $# -ne 1 ]]; then
    echo "./redis-cli-workload.sh num_ops"
    exit 1
fi

function feed_workload {
    local op_seq
    op_seq=$(seq 1 "$1")
    for i in $op_seq; do
        echo "set key-$i $i"
    done
    for i in $op_seq; do
        echo "get key-$i"
    done
    for i in $op_seq; do
        echo "del key-$i"
    done
}

num_ops=$1

feed_workload "$num_ops" | /root/redis/src/redis-cli >/dev/null
sleep 1
echo "shutdown" | /root/redis/src/redis-cli
