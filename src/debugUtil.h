#ifndef DEBUGUTIL_H
#define DEBUGUTIL_H
#include <stdio.h> 

// Function declarations
int initDebuggerFiles();
void closeDebuggerFiles(); // Added cleanup function

// Always declare the debug file pointer for ERR_PRINT
extern FILE *_private_debug_file;

#ifdef DEBUG
    #include <signal.h> // Breakpoint for debugging
    #define DEBG_PRINT(...) do { \
        if (_private_debug_file) { \
            fprintf(_private_debug_file, __VA_ARGS__); \
            fflush(_private_debug_file); \
        } \
    } while (0)
    #define SET_BREAK_POINT raise(SIGTRAP)
#else
    #define DEBG_PRINT(...) do {} while (0)
    #define SET_BREAK_POINT
#endif

// ERR_PRINT always writes to debug.log if available, otherwise stderr
#define ERR_PRINT(...) do { \
    if (_private_debug_file) { \
        fprintf(_private_debug_file, "[ERROR:] " __VA_ARGS__); \
        fflush(_private_debug_file); \
    } else { \
        fprintf(stderr, "[ERROR:] " __VA_ARGS__); \
    } \
} while (0)

#ifdef PROFILE
    extern FILE *_private_profiler_file;
    #define PROFILER_PRINT(...) do { \
        if (_private_profiler_file) { \
            fprintf(_private_profiler_file, __VA_ARGS__); \
            fflush(_private_profiler_file); \
        } \
    } while (0)
#else
    #define PROFILER_PRINT(...) do {} while (0)
#endif

#endif