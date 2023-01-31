#! /bin/bash
SCRIPT=$(basename "${BASH_SOURCE[0]}")
BOLD=$(tput bold)
OFF=$(tput sgr0)

RATE=1
PM_MOUNT=/mnt/dbpmemfs
OUT=$(mktemp)
TARGET=''

function main() {
    parse_args "$@"

    rm -f "$OUT"
    export -f run_and_collect
    export RATE
    export PM_MOUNT
    export OUT
    time_out=$(mktemp)

    echo run_and_collect "$TARGET" | /usr/bin/time -p -o "$time_out" /bin/bash

    time=$(grep "real" "$time_out" | cut -d " " -f 2)

    rest=$(get_summary "$OUT")

    echo "$OUT"
    printf "%s,%s\n" "$time" "$rest" | tee "$OUT".summary

    rm -f "$time_out"
}

function get_summary() {
    local file=$1
    # shellcheck disable=SC2016
    awk_command='
    BEGIN {
        pm=0; ram=0; cpu=0; load1=0; load5=0; load15=0; actual_mem=0
    } NR>1 {
        pm=$1>pm?$1:pm
        ram=$2>ram?$2:ram
        cpu=$3>cpu?$3:cpu
        load1=$4>load1?$4:load1
        load5=$5>load5?$5:load5
        load15=$6>load15?$6:load15
        actual_mem=$7>actual_mem?$7:actual_mem
    } END {
        print pm","ram/1024","cpu","load1","load5","load15","actual_mem
    }'

    awk -F "," "$awk_command" "$file"
}

function run_and_collect() {
    set -um

    # monitor the resource usage in the background.
    pgid=$(ps -o pgid= $$ | xargs)
    (
        echo "pm,ram,cpu,load1,load5,load15" >>"$OUT"
        while mem_cpu=$(/bin/ps --no-headers -o rss=,%cpu -"$pgid"); do
            # /bin/ps -o %cpu,rss= -$pgid | tail ->&2
            mem_cpu=$(echo "$mem_cpu" | tail -n 1 | xargs | tr " " ",")
            pm=$(df -m | grep "$PM_MOUNT" | awk '{print $3}')
            load=$(awk '{print $1","$2","$3}' /proc/loadavg)
            actual_mem=$(free -m | awk 'NR == 2 { print $3 }')
            echo "$pm,$mem_cpu,$load,$actual_mem" >>"$OUT"
            sleep "$RATE"
        done
    ) &

    # run the given command
    exec "$@"
}

function parse_args {
    while getopts r:o:m:s:h FLAG; do
        case $FLAG in
        r) readonly RATE=$OPTARG ;;
        o) readonly OUT=$OPTARG ;;
        m) readonly PM_MOUNT=$OPTARG ;;
        s)
            local file=$OPTARG
            get_summary "$file"
            exit 0
            ;;
        h)
            show_help
            exit 0
            ;;
        \?) #unrecognized option - show help
            echo -e \\n"Option -${BOLD}$OPTARG${OFF} not allowed."
            show_help
            exit 1
            ;;
        esac
    done

    # Get the remainder
    shift $((OPTIND - 1))
    readonly TARGET="$*"
    if [ -z "$TARGET" ]; then # if there is no target invocation
        echo -e "${BOLD}Missing target invocation."\\n
        show_help
    fi

}

function show_help {
    echo -e "Usage: ${BOLD}$SCRIPT [-r rate] [-o path] [-m pm_mount] command${OFF}"
    echo -e "Resource collection (cpu, ram, pm).\n"
    echo -e "  -o path      Output file."
    echo -e "  -m path      PM mount path, used to collect PM usage."
    echo -e "               Defaults to ${BOLD}/mnt/pmem0${OFF}."
    echo -e "  -r           Collection rate (in seconds)."
    echo -e "               Defaults to ${BOLD}1${OFF}."
    echo -e "  -s file      Get summary of file."
    echo -e "  -h           Display this help message."\\n
}

main "$@"
