#ifndef MUMAK_LOGGER_H
#define MUMAK_LOGGER_H

#define DEBUG
#ifdef DEBUG
#define debug(...) fprintf(stderr, "[DBG] " __VA_ARGS__)
static unsigned long pm_stores, stores, flushes, fences, rmw;
#define COUNT_PM_STORE pm_stores++
#define COUNT_STORE stores++
#define COUNT_FLUSH flushes++
#define COUNT_FENCE fences++
#define COUNT_RMW rmw++
#define OUTPUT_INSTRUCTION_COUNT                                              \
    debug(                                                                    \
        "stores: %lu, pm stores: %lu (%lf%%), flushes: %lu, fences: %lu, "    \
        "rmw: %lu\n",                                                         \
        stores, pm_stores, (double)pm_stores / stores * 100, flushes, fences, \
        rmw);
#else
#define debug(...)
#define COUNT_PM_STORE
#define COUNT_STORE
#define COUNT_FLUSH
#define COUNT_FENCE
#define COUNT_RMW
#define OUTPUT_INSTRUCTION_COUNT
#endif  // !DEBUG

#endif  // !MUMAK_LOGGER_H
