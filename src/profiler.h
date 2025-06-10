#ifndef PROFILER_H
#define PROFILER_H

#include <time.h>
#include "debugUtil.h"

/**
 * Simple profiler for measuring time of some operation
 */

volatile static clock_t _private_start_time = -1;
/**
 * Start the profiler.
 */
static inline void profilerStart() {
    if(_private_start_time == -1){
        _private_start_time = clock();
    }
}

/**
 * Stop the profiler, calculate and print elapsed time.
 * >> returns the elapsed time in seconds.
 */
static inline double profilerStop(char* nameOfOperation) {
    volatile clock_t end_time = clock();
    volatile double elapsed_time = -1;
    if (_private_start_time != -1 || end_time != -1){
        elapsed_time = ((double) (end_time - _private_start_time)) / (double)CLOCKS_PER_SEC;
        PROFILER_PRINT("%s_took_(s)_at_(s),%f,%f,\n", nameOfOperation, elapsed_time, _private_start_time/(double)CLOCKS_PER_SEC);
    } else{
        volatile clock_t end_time = clock();
        elapsed_time = ((double) (end_time - _private_start_time)) / (double)CLOCKS_PER_SEC;
        PROFILER_PRINT("%s_ERROR: start:%f, end:%f\n", nameOfOperation, elapsed_time, _private_start_time);
    }
    _private_start_time = -1;
    return elapsed_time;
}

#endif