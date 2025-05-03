#define _XOPEN_SOURCE_EXTENDED
#include <locale.h>
#include <wchar.h>
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include "textStructure.h"

#define BUFFER_SIZE 1024

static Atomic inputBuffer[BUFFER_SIZE];
static int bufLen = 0;      // total bytes in buffer
static int cursorPos = 0;   // insertion index

wchar_t* utf8_to_wchar(const Atomic* utf8, int length) {
    wchar_t* wstr = malloc((length + 1) * sizeof(wchar_t));
    if (!wstr) return NULL;

    mbstate_t state;
    memset(&state, 0, sizeof(state));
    const char* src = (const char*)utf8;
    size_t result = mbsrtowcs(wstr, &src, length, &state);
    if (result == (size_t)-1) {
        free(wstr);
        return NULL;
    }
    wstr[result] = L'\0';
    return wstr;
}

int main() {
    /* Locale + simple mouse‐click reporting */
    setlocale(LC_ALL, "en_US.UTF-8");
    printf("\033[?1000h");  // enable button‐click events
    fflush(stdout);

    initscr();
    raw();
    noecho();
    curs_set(1);
    clearok(stdscr, TRUE);
    keypad(stdscr, TRUE);

    mouseinterval(0);
    mousemask(BUTTON1_CLICKED, NULL);

    Sequence* seq = Empty(LINUX);
    if (!seq) {
        printf("\033[?1000l");
        endwin();
        fprintf(stderr, "Failed to initialize sequence\n");
        return 1;
    }

    int ch;
    MEVENT event;
    while ((ch = getch()) != KEY_F(1)) {
        if (ch == KEY_MOUSE && getmouse(&event) == OK) {
            int click_line = event.y - 5;
            int click_col  = event.x;
            if (click_line >= 0) {
                // 1) find byte index of line start
                int line = 0, idx = 0;
                for (; idx < bufLen && line < click_line; ++idx) {
                    if (inputBuffer[idx] == '\n') ++line;
                }
                // 2) advance to column (naïve byte-per-col)
                int col = 0;
                while (idx < bufLen && col < click_col) {
                    unsigned char c = (unsigned char)inputBuffer[idx];
                    int step = 1;  // for true UTF‑8 you could decode wchar and use its width
                    idx += step;
                    col += 1;
                }
                // clamp
                cursorPos = idx;
            }
        }
        else if (ch == KEY_RESIZE) {
            resizeterm(0, 0);
        }
        else if (ch >= 0 && bufLen < BUFFER_SIZE - 4) {
            // INSERT mode: shift tail right one byte
            memmove(inputBuffer + cursorPos + 1,
                    inputBuffer + cursorPos,
                    bufLen - cursorPos);
            inputBuffer[cursorPos] = (Atomic)ch;
            bufLen++;
            cursorPos++;
        }

        /* redraw */
        clear();

        // Convert entire buffer to wide
        wchar_t* wstr = utf8_to_wchar(inputBuffer, bufLen);

        // Count lines in buffer
        int total_lines = 1;
        for (int i = 0; i < bufLen; ++i)
            if (inputBuffer[i] == '\n') ++total_lines;

        // Determine cursor's logical line & column in wide chars
        int curLine = 0, lineStart = 0;
        for (int i = 0; i < cursorPos; ++i) {
            if (inputBuffer[i] == '\n') {
                curLine++;
                lineStart = i + 1;
            }
        }
        int curByteLen = cursorPos - lineStart;
        int curCol = 0;
        if (curByteLen > 0) {
            wchar_t* cw = utf8_to_wchar(inputBuffer + lineStart, curByteLen);
            if (cw) {
                curCol = wcslen(cw);
                free(cw);
            }
        }

        // Top bar
        for (int i = 0; i < 4; ++i) {
            move(i, 0);
            clrtoeol();
        }
        mvaddstr(0, 0, "F1:Exit | Ctrl+S:Save | Ctrl+O:Open | Ctrl+N:New");
        char stats[128];
        int stats_len = snprintf(stats, sizeof(stats),
                                 "Line:%d Col:%d Chars:%d | UTF-8 | Ins",
                                 curLine+1, curCol+1, total_lines);
        mvprintw(0, COLS - stats_len - 1, "%s", stats);

        // Separator
        mvhline(4, 0, '=', COLS);

        // Render each line
        if (wstr) {
            int y = 5;
            wchar_t* ptr = wstr;
            while (*ptr) {
                wchar_t* nl = wcschr(ptr, L'\n');
                size_t len = nl ? (nl - ptr) : wcslen(ptr);
                mvaddnwstr(y++, 0, ptr, COLS);
                clrtoeol();
                if (!nl) break;
                ptr = nl + 1;
            }
            free(wstr);
        }

        // Place cursor
        int drawY = 5 + curLine;
        int drawX = curCol >= COLS ? COLS - 1 : curCol;
        move(drawY, drawX);
        refresh();
    }

    /* cleanup */
    printf("\033[?1000l");
    endwin();
    return 0;
}
