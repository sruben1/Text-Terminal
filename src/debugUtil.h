#ifndef DEBUG_H
#define DEBUG_H
#include <stdio.h> 

#define ERR_PRINT(...) fprintf(stderr, "[ERROR:] " __VA_ARGS__)

#ifdef DEBUG
    #include <signal.h> // Breakpoint for debugging
    #define DEBG_PRINT(...) printf(__VA_ARGS__)
    #define SET_BREAK_POINT raise(SIGTRAP)
#else
    #define DEBG_PRINT(...) do {} while (0)
    #define SET_BREAK_POINT
#endif

#ifdef PROFILE
    #define PROFILER_PRINT(...) printf(__VA_ARGS__)
#else
    #define PROFILER_PRINT(...) do {} while (0)
#endif

#endif