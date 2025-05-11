// gui.c - ncurses-based GUI for the piece-table editor

#define _XOPEN_SOURCE_EXTENDED 1  // for wide-character functions
#include <ncurses.h>
#undef move // remove ncurses macro before including our headers

#include <locale.h>
#include <stdlib.h>
#include <wchar.h>
#include <string.h>
#include "textStructure.h"
#include "guiUtilities.h"
#include "debugUtil.h"

#define CTRL_KEY(k) ((k) & 0x1f)

static Sequence *activeSequence = NULL;
static int cursorX = 0, cursorY = 0;
static Position topLineAtomic = 0;
static Position lineStartAtomic[1024];
static int screenRows, screenCols;

// Forward declarations
void init_editor(void);
void close_editor(void);
void refresh_screen(void);
void process_input(void);
void draw_rows(void);
int fetch_line(Position startAtomic, wchar_t **outLine, int *outCharCount);
void insert_character(wchar_t wc);

int main(int argc, char *argv[]) {
    setlocale(LC_ALL, "");
    init_editor();

    activeSequence = empty(LINUX);
    setLineStatsNotUpdated();
    topLineAtomic = 0;

    while (1) {
        refresh_screen();
        process_input();
    }

    close_editor();
    return 0;
}

void init_editor(void) {
    if (!initscr()) {
        perror("initscr");
        exit(EXIT_FAILURE);
    }
    raw();
    noecho();
    keypad(stdscr, TRUE);
    getmaxyx(stdscr, screenRows, screenCols);
    if (screenRows > 1024) screenRows = 1024;
}

void close_editor(void) {
    endwin();
    if (activeSequence) {
        close(activeSequence, true);
    }
}

void refresh_screen(void) {
    erase();
    draw_rows();
    // move cursor with curses function
    wmove(stdscr, cursorY, cursorX);
    // Status bar
    int absLine = getGeneralLineNbr(cursorY);
    attron(A_REVERSE);
    mvprintw(screenRows - 1, 0,
        "Ln %d, Col %d   Ctrl-Q to quit",
        (absLine >= 0 ? absLine : 0) + 1,
        cursorX + 1
    );
    attroff(A_REVERSE);
    refresh();
}

void draw_rows(void) {
    Position pos = topLineAtomic;
    for (int y = 0; y < screenRows - 1; y++) {
        wchar_t *line = NULL;
        int charCount = 0;
        int nextBytes = fetch_line(pos, &line, &charCount);
        if (nextBytes <= 0 || !line) {
            mvaddch(y, 0, '~');
            updateLine(y, pos, 0);
        } else {
            // use mvwaddwstr to display wide string
            mvwaddwstr(stdscr, y, 0, line);
            updateLine(y, pos, charCount);
            pos += nextBytes;
        }
        lineStartAtomic[y] = pos;
        free(line);
    }
}

/**
 * Reads one line starting at startAtomic.
 * Returns bytes consumed or -1 on error. Also returns char count.
 */
int fetch_line(Position startAtomic, wchar_t **outLine, int *outCharCount) {
    if (!outLine || !outCharCount) return -1;
    *outLine = NULL;
    *outCharCount = 0;

    Position pos = startAtomic;
    int totalChars = 0, consumed = 0;
    bool done = false;
    Atomic *block;
    LineBidentifier lb = getCurrentLineBidentifier();

    // First pass: count UTF-8 chars and bytes
    while (!done) {
        int blockSize = getItemBlock(activeSequence, pos + consumed, &block);
        if (blockSize <= 0 || !block) break;
        for (int i = 0; i < blockSize; i++) {
            Atomic c = block[i];
            consumed++;
            if ((c & 0xC0) != 0x80) totalChars++;
            if (c == lb || consumed >= screenCols) {
                done = true;
                break;
            }
        }
    }
    if (consumed <= 0) return -1;

    // Collect bytes
    Atomic *buffer = malloc(consumed);
    if (!buffer) return -1;
    int bufferIdx = 0;
    int remaining = consumed;
    Position currentPos = startAtomic;
    while (remaining > 0) {
        int blockSize = getItemBlock(activeSequence, currentPos, &block);
        if (blockSize <= 0 || !block) {
            free(buffer);
            return -1;
        }
        int copySize = (blockSize < remaining) ? blockSize : remaining;
        memcpy(buffer + bufferIdx, block, copySize);
        bufferIdx += copySize;
        remaining -= copySize;
        currentPos += copySize;
    }

    // Convert to wide string via guiUtilities
    wchar_t *wline = utf8_to_wchar(buffer, consumed, totalChars);
    free(buffer);
    if (!wline) return -1;

    *outLine = wline;
    *outCharCount = totalChars;
    return consumed;
}

void process_input(void) {
    int c = getch();
    switch (c) {
    case CTRL_KEY('q'):
        close_editor();
        exit(0);
        break;
    case KEY_UP:
        if (cursorY > 0) cursorY--;
        break;
    case KEY_DOWN:
        if (cursorY < screenRows - 2) cursorY++;
        break;
    case KEY_LEFT:
        if (cursorX > 0) cursorX--;
        break;
    case KEY_RIGHT: {
        int maxCol = getUtfNoControlCharCount(cursorY);
        if (maxCol < 0) maxCol = 0;
        if (cursorX < maxCol) cursorX++;
        break;
    }
    default:
        if (c >= 32 || c == '\n') {
            insert_character((wchar_t)c);
        }
        break;
    }
}

void insert_character(wchar_t wc) {
    int atomicPos = getAbsoluteAtomicIndex(cursorY, cursorX, activeSequence);
    if (atomicPos < 0) return;

    wchar_t buf[2] = {wc, L'\0'};
    if (insert(activeSequence, atomicPos, buf) == 1) {
        cursorX++;
        setLineStatsNotUpdated();
    }
}
