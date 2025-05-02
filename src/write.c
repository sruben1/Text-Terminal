#define _XOPEN_SOURCE_EXTENDED  // Enable wide character support
#include <locale.h>    // UTF-8 locale configuration
#include <wchar.h>     // Wide character handling
#include <ncurses.h>   // Terminal UI functions
#include <stdlib.h>    // Memory management
#include <string.h>    // String operations
#include "textStructure.h"  // Custom text structure

#define BUFFER_SIZE 1024  // Input buffer capacity
static Atomic inputBuffer[BUFFER_SIZE]; // Raw input storage
static int bufferPos = 0;  // Current buffer position


wchar_t* utf8_to_wchar(const Atomic* utf8, int length) {
    wchar_t* wstr = malloc((length + 1) * sizeof(wchar_t));
    if (!wstr) return NULL;
    
    mbstate_t state;                // Conversion state tracker
    memset(&state, 0, sizeof(state)); // Reset conversion state
    size_t result = mbsrtowcs(wstr, (const char**)&utf8, length, &state);
    if (result == (size_t)-1) {     // Handle conversion errors
        free(wstr);
        return NULL;
    }
    wstr[result] = L'\0';  // Wide string null terminator
    return wstr;
}

int main() {
    /* NCurses Initialization */
    setlocale(LC_ALL, "en_US.UTF-8");  // Set UTF-8 locale
    initscr();   // Initialize curses mode
    raw();       // Direct input processing
    keypad(stdscr, TRUE);  // Enable special keys
    noecho();    // Don't echo typed characters
    curs_set(1); // Visible cursor
    clearok(stdscr, TRUE); // Clear screen on resize

    /* Text Structure Initialization */
    Sequence* seq = Empty(LINUX);
    if (!seq) {
        endwin();
        fprintf(stderr, "Failed to initialize sequence\n");
        return 1;
    }

    int ch;  // Input character storage
    while ((ch = getch()) != KEY_F(1)) {  // Main loop until F1 pressed
        /* Input Handling */
        if (ch == KEY_RESIZE) {
            resizeterm(0, 0);  // Handle terminal resize
        } else if (bufferPos < BUFFER_SIZE - 4) {  // Leave space for 4-byte UTF-8
            inputBuffer[bufferPos++] = (Atomic)ch;  // Store character
        }

        clear();  // Clear screen for redraw

        /* Text Metrics Calculation */
        wchar_t* wstr = utf8_to_wchar(inputBuffer, bufferPos);
        int lines = 1;              // Total lines count
        int current_line_start = 0;  // Start index of current line
        int current_col = 0;         // Column in current line (wide chars)
        int total_chars = 0;         // Total visible characters

        if (wstr) {
            total_chars = wcslen(wstr);  // Get wide character count
            
            // Calculate line breaks using original buffer
            for (int i = 0; i < bufferPos; ++i) {
                if (inputBuffer[i] == '\n') {  // Manual line breaks only
                    lines++;
                    current_line_start = i + 1;
                }
            }

            // Convert current line for accurate column count
            int current_line_length = bufferPos - current_line_start;
            wchar_t* current_line_wstr = utf8_to_wchar(
                inputBuffer + current_line_start, 
                current_line_length
            );
            if (current_line_wstr) {
                current_col = wcslen(current_line_wstr);
                free(current_line_wstr);  // Free temporary conversion
            }
        }

        /* Interface Drawing */
        // Clear top bar area
        for (int i = 0; i < 4; i++) {
            move(i, 0);
            clrtoeol();  // Reset top bar lines
        }
        
        // Left-aligned shortcuts
        mvaddstr(0, 0, "F1:Exit | Ctrl+S:Save | Ctrl+O:Open | Ctrl+N:New");
        
        // Right-aligned status info
        char stats[128];
        int stats_len = snprintf(stats, sizeof(stats), 
            "Line:%d Col:%d Chars:%d | UTF-8 | Ins",
            lines, current_col + 1, total_chars
        );
        mvprintw(0, COLS - stats_len - 1, "%s", stats);

        /* Text Display (No Wrapping) */
        mvhline(4, 0, '=', COLS);  // Header separator
        if (wstr) {
            int screen_line = 5;  // Starting line for text
            wchar_t* line = wstr;
            
            while (*line) {  // Process all logical lines
                // Find next newline or end of string
                wchar_t* end = wcschr(line, L'\n');
                size_t line_length = end ? (end - line) : wcslen(line);

                // Display max COLS characters without wrapping
                mvaddnwstr(screen_line, 0, line, COLS);
                clrtoeol();  // Clear any overflow from previous state

                if (end) {  // Move to next logical line
                    line = end + 1;
                    screen_line++;
                } else {
                    break;  // Exit loop at end of content
                }
            }
            free(wstr);  // Free main conversion buffer
        }

        /* Cursor Positioning */
        int cursor_x = current_col;
        // Clamp cursor to visible area (no wrapping)
        if (cursor_x >= COLS) cursor_x = COLS - 1;
        // Calculate vertical position (header offset + line count)
        int cursor_y = 5 + (lines - 1);
        move(cursor_y, cursor_x);

        refresh();  // Update display
    }

    endwin();  // Restore terminal
    return 0;
}