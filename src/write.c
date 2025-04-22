#define _XOPEN_SOURCE_EXTENDED  // wide character support (mbstate_t, mbsrtowcs, addwstr.)
#include <locale.h>   // utf8 locale configuration
#include <wchar.h>    // wide character
#include <ncurses.h>  // for terminal UI
#include <stdlib.h>   // for malloc/free
#include <string.h>   // for memset       
#include "textStructure.h"

// custom buffer to store input (since Insert not implemented yet)
#define BUFFER_SIZE 1024
static Atomic inputBuffer[BUFFER_SIZE];
static int bufferPos = 0;

// function to convert utf8 bytes to wide string
wchar_t* utf8_to_wchar(const Atomic* utf8, int length) {
    wchar_t* wstr = malloc((length + 1) * sizeof(wchar_t));
    if (!wstr) return NULL;
    
    mbstate_t state;                    // tracks conversion state
    memset(&state, 0, sizeof(state));   // reset state to initial
    size_t result = mbsrtowcs(wstr, (const char**)&utf8, length, &state);
    if (result == (size_t)-1) {         // check conversion errors
        free(wstr);
        return NULL;
    }
    wstr[result] = L'\0'; // null-terminate wide string
    return wstr;
}

int main() {
    setlocale(LC_ALL, "en_US.UTF-8");
    initscr(); // initialize ncurses
    raw(); // line buffering disabled, no need for enter key
    keypad(stdscr, TRUE); // enable special keys
    noecho(); // don't echo input
    curs_set(1); // visible cursor

    // initialize sequence
    Sequence* seq = Empty(LINUX);
    if (!seq) {
        endwin();
        fprintf(stderr, "Failed to initialize sequence\n");
        return 1;
    }

    int ch;
    while ((ch = getch()) != KEY_F(1)) { // exit with F1
        if (bufferPos >= BUFFER_SIZE - 4) break; // prevent overflow (-4 = space for 4 bytes)
        
        // convert to UTF-8 (simplified for ASCII)
        inputBuffer[bufferPos++] = (Atomic)ch;
        
        // display input
        clear();
        wchar_t* wstr = utf8_to_wchar(inputBuffer, bufferPos);
        if (wstr) {
            addwstr(wstr); // dsisplay wide string
            free(wstr);
        }
        refresh(); // updates terminal display after change
    }

    endwin(); // restore terminal after closing
    return 0;
}