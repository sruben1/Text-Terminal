#include "debugUtil.h"

// Forward declaration:
static int _initDebugger();
static int _initProfiler();
static void _closeDebugger();
static void _closeProfiler();

int initDebuggerFiles() {
    return _initDebugger() + _initProfiler();
}

void closeDebuggerFiles() {
    _closeDebugger();
    _closeProfiler();
}

// Always define the debug file pointer (for ERR_PRINT)
FILE *_private_debug_file = NULL;

static int _initDebugger() {
        _private_debug_file = fopen("./debug.log", "w");
        if (_private_debug_file == NULL) {
            fprintf(stderr, "Failed to open debug.log for writing\n");
            return -1;
        }
        return 0;
    }

#ifdef DEBUG

    static void _closeDebugger() {
        if (_private_debug_file) {
            fclose(_private_debug_file);
            _private_debug_file = NULL;
        }
    }
#else
    static int _initDebugger() {
        _private_debug_file = fopen("./debug.log", "w");
        return 0;
    }

    static void _closeDebugger() {
        if (_private_debug_file) {
            fclose(_private_debug_file);
            _private_debug_file = NULL;
        }
    }
#endif

#ifdef PROFILE
    FILE *_private_profiler_file = NULL;

    static int _initProfiler() {
        _private_profiler_file = fopen("./profiler.log", "w");
        if (_private_profiler_file == NULL) {
            fprintf(stderr, "Failed to open profiler.log for writing\n");
            return -1;
        }
        return 0;
    }

    static void _closeProfiler() {
        if (_private_profiler_file) {
            fclose(_private_profiler_file);
            _private_profiler_file = NULL;
        }
    }
#else
    static int _initProfiler() {
        return 0;
    }

    static void _closeProfiler() {}
#endif