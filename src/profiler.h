#ifndef PROFILER_H
#define PROFILER_H

#include <time.h>
#include "debugUtil.h"

/**
 * Simple profiler for measuring time of some operation
 */

static clock_t profiler_start_time;

/**
 * Start the profiler.
 */
static inline void profilerStart() {
    if (profiler_start_time == -1){
        profiler_start_time = clock();
    }
}

/**
 * Stop the profiler, calculate and print elapsed time.
 * >> returns the elapsed time in seconds.
 */
static inline double profilerStop(char* nameOfOperation) {
    double elapsed_time = -1;
    if (profiler_start_time != -1){
        clock_t end_time = clock();
        elapsed_time = (end_time - profiler_start_time) / (double)CLOCKS_PER_SEC;
        PROFILER_PRINT("%s took: %f seconds\n", nameOfOperation, elapsed_time);
    }
    profiler_start_time = -1;
    return elapsed_time;
}

#endif