#define _XOPEN_SOURCE_EXTENDED  // Enable wide character support
#include <locale.h>    // For UTF-8 locale configuration
#include <wchar.h>     // Wide character handling
#include <ncurses.h>   // Terminal UI library
#include <stdlib.h>    // Memory allocation
#include <string.h>    // String operations
#include "textStructure.h"  // Custom text structure definitions

#define BUFFER_SIZE 1024  // Maximum input buffer size
static Atomic inputBuffer[BUFFER_SIZE];  // Storage for user input
static int bufferPos = 0;  // Current position in input buffer

// Convert UTF-8 bytes to wide character string
wchar_t* utf8_to_wchar(const Atomic* utf8, int length) {
    wchar_t* wstr = malloc((length + 1) * sizeof(wchar_t));
    if (!wstr) return NULL;
    
    mbstate_t state;              // Conversion state tracking
    memset(&state, 0, sizeof(state));  // Initialize state
    size_t result = mbsrtowcs(wstr, (const char**)&utf8, length, &state);
    if (result == (size_t)-1) {   // Check for conversion errors
        free(wstr);
        return NULL;
    }
    wstr[result] = L'\0';  // Null-terminate wide string
    return wstr;
}

int main() {
    // Initialize locale and terminal
    setlocale(LC_ALL, "en_US.UTF-8");  // Set UTF-8 locale
    initscr();   // Start curses mode
    raw();       // Disable line buffering
    keypad(stdscr, TRUE);  // Enable special keys
    noecho();    // Don't echo input characters
    curs_set(1); // Visible cursor
    clearok(stdscr, TRUE);  // Allow screen clearing on resize

    // Initialize text sequence structure
    Sequence* seq = Empty(LINUX);
    if (!seq) {
        endwin();
        fprintf(stderr, "Failed to initialize sequence\n");
        return 1;
    }

    int ch;  // Store input character
    while ((ch = getch()) != KEY_F(1)) {  // Main loop until F1 pressed
        // Handle terminal resize
        if (ch == KEY_RESIZE) {
            resizeterm(0, 0);  // Update ncurses with new dimensions
        } 
        // Store valid input
        else if (bufferPos < BUFFER_SIZE - 4) {  // Leave space for multi-byte chars
            inputBuffer[bufferPos++] = (Atomic)ch;
        }

        clear();  // Clear screen for redraw

        /* ========== TEXT PROCESSING SECTION ========== */
        // Convert entire buffer to wide string once for display
        wchar_t* wstr = utf8_to_wchar(inputBuffer, bufferPos);
        
        // Initialize text metrics
        int lines = 1;              // Total lines (starts at 1)
        int current_line_start = 0; // Start index of current line
        int current_col = 0;        // Column in current line
        int total_chars = 0;        // Total visible characters

        if (wstr) {
            total_chars = wcslen(wstr);  // Get wide char count
            
            // Calculate line information using original buffer
            for (int i = 0; i < bufferPos; ++i) {
                if (inputBuffer[i] == '\n') {  // Count newlines
                    lines++;
                    current_line_start = i + 1;
                }
            }

            // Convert only current line for accurate column count
            int current_line_length = bufferPos - current_line_start;
            wchar_t* current_line_wstr = utf8_to_wchar(
                inputBuffer + current_line_start, 
                current_line_length
            );
            if (current_line_wstr) {
                current_col = wcslen(current_line_wstr);  // Wide char columns
                free(current_line_wstr);  // Free temporary line conversion
            }
        }

        /* ========== INTERFACE DRAWING SECTION ========== */
        // Clear top bar area
        for (int i = 0; i < 4; i++) {
            move(i, 0);
            clrtoeol();  // Clear to end of line
        }
        
        // Draw shortcut list (left-aligned)
        mvaddstr(0, 0, "F1:Exit | Ctrl+S:Save | Ctrl+O:Open | Ctrl+N:New");
        mvaddstr(1, 0, "Ctrl+Z:Undo | Ctrl+Y:Redo | Ctrl+F:Find | Ctrl+H:Replace");

        // Format status information (right-aligned)
        char stats[128];
        int stats_len = snprintf(stats, sizeof(stats), 
            "Line:%d Col:%d Chars:%d | UTF-8 | Ins",
            lines, current_col + 1, total_chars
        );
        mvprintw(0, COLS - stats_len - 1, "%s", stats);

        // Draw separator line between header and text
        mvhline(4, 0, '=', COLS);

        // Display main text content
        if (wstr) {
            mvaddwstr(5, 0, wstr);  // Start text at line 5
            free(wstr);  // Free the main wide string conversion
        }

        /* ========== CURSOR POSITIONING ========== */
        // Calculate vertical position: header lines + lines of text
        int cursor_y = 5 + (lines - 1);  
        // Horizontal position: wide char column count
        move(cursor_y, current_col);
        
        refresh();  // Update screen
    }

    endwin();  // Restore terminal
    return 0;
}