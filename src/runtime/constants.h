// Copyright 2025 João Gonçalves
#ifndef LIBPIFRRT_CONSTANTS
#define LIBPIFRRT_CONSTANTS

#define ENV_MINIMAL         "PIFR_MINIMAL"
#define ENV_SAMPLING        "PIFR_SAMPLING" // 0: off, X: sampling rate
#define ENV_REPORT          "PIFR_REPORT" // 0: no report, 1: report, 2: report w/ backtraces
#define ENV_DELAY           "PIFR_DELAY"
#define ENV_DECAY           "PIFR_DECAY"
#define ENV_TRACING         "PIFR_TRACING"
#define ENV_PROFILING       "PIFR_PROFILING"
#define ENV_SEED            "PIFR_SEED"

#define DEFAULT_MINIMAL     0
#define DEFAULT_SAMPLING    0
#define DEFAULT_REPORT      1
#define DEFAULT_DELAY       0
#define DEFAULT_DECAY       5
#define DEFAULT_TRACING     0
#define DEFAULT_PROFILING   0
#define DEFAULT_SEED        42069

#define RANDOM_SIZE 10000

#define BUGS_PATH "/mnt/ramdisk/bugs.txt"
#define BUGS_DATA_PATH "/mnt/ramdisk/bugs.bin"
#define TIMES_PATH "/mnt/ramdisk/times.txt"
#define TRACE_PATH "/mnt/ramdisk/trace.bin"
#define OPENS_PATH "/mnt/ramdisk/opens.txt"
// "r/w race on %x (%x - %x) (%d x %d)"
// mem_addr pifr_id_a pifr_id_b diff_thread
#define BUG_REPORT_FORMAT "%p %d %d %d\n"

#define GETENV_OR_DEFAULT(name)  \
({ \
    const char *val = getenv(ENV_##name); \
    val ? atoi(val) : (DEFAULT_##name); \
})

#endif
