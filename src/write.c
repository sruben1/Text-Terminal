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
    curs_set(1); // visible blinking cursor
    clearok(stdscr, TRUE); // force full redraw after resize

    // initialize sequence
    Sequence* seq = Empty(LINUX);
    if (!seq) {
        endwin();
        fprintf(stderr, "Failed to initialize sequence\n");
        return 1;
    }

    int ch;
    while ((ch = getch()) != KEY_F(1)) { // exit with F1
        if (ch == KEY_RESIZE){
            resizeterm(0,0); // tells ncurses to pick up new lines & cols
        } else{
            if (bufferPos >= BUFFER_SIZE - 4) break; // prevent overflow (-4 = space for 4 bytes)
        
            // convert to UTF-8 (simplified for ASCII)
            inputBuffer[bufferPos++] = (Atomic)ch;
        }
        
        // display interface
        clear();

        for (int i = 0; i < 4; i++) {
            move(i, 0);
            clrtoeol();
        }
        mvaddstr(0, 0, "F1:Exit | Ctrl+S:Save | Ctrl+O:Open | Ctrl+N:New");
        mvaddstr(1, 0, "Ctrl+Z:Undo | Ctrl+Y:Redo | Ctrl+F:Find | Ctrl+H:Replace");
        
        mvhline(4, 0, '=', COLS);

        /* Main Text Area (below header separator) */
        wchar_t* wstr = utf8_to_wchar(inputBuffer, bufferPos);
        if (wstr) {
            mvaddwstr(5, 0, wstr); // start at line 5
            free(wstr);
        }

        //statistics
        int lines = 1, current_line_start = 0;
        for (int i = 0; i < bufferPos; ++i) {
            if (inputBuffer[i] == '\n') {
                lines++;
                current_line_start = i + 1;
            }
        }
        int current_col = bufferPos - current_line_start;
        int total_chars = bufferPos;

        /* - 4 lines for statistics */
        int bottom_start = LINES - 4;
        // Footer separator 
        mvhline(bottom_start - 1, 0, '=', COLS);
        
        for (int i = 0; i < 4; i++) {
            move(bottom_start + i, 0);
            clrtoeol();
        }
        mvprintw(bottom_start, 0, "Line: %d | Col: %d | Total Chars: %d", lines, current_col + 1, total_chars);
        mvprintw(bottom_start + 1, 0, "Encoding: UTF-8 | Mode: Insert");

        /* Cursor Positioning */
        int cursor_y = 5 + (lines - 1);  
        int cursor_x = current_col;
        move(cursor_y, cursor_x);

        refresh(); // updates terminal display after change
    }

    endwin(); // restore terminal after closing
    return 0;
}