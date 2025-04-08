#include <ncurses.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINES 1000
#define MAX_COLS 1024

typedef struct {
    char **lines;
    int row, col;
    int num_lines;
    int max_line_length;
    char *filename;
} Editor;

Editor editor;

void init_editor() {
    editor.lines = malloc(MAX_LINES * sizeof(char *));
    for(int i = 0; i < MAX_LINES; i++) {
        editor.lines[i] = calloc(MAX_COLS, sizeof(char));
    }
    editor.row = 0;
    editor.col = 0;
    editor.num_lines = 1;
    editor.max_line_length = 0;
    editor.filename = "untitled.txt";
}

void init_ncurses() {
    initscr();
    raw();
    keypad(stdscr, TRUE);
    noecho();
    curs_set(1);
}

void draw_editor() {
    clear();
    
    // header
    mvprintw(0, 0, " Ctrl+S: Save     Ctrl+F: Find");
    mvprintw(1, 0, " Ctrl+N: New     Ctrl+O: Open");
    mvprintw(2, 0, " F1: Exit");
    for(int i = 3; i < 6; i++) {  //space for other shortcuts
        mvprintw(i, 0, " --");
    }
    
    mvhline(6, 0, '=', COLS);

    // text dimens.
    int text_start_row = 7;
    int text_rows = LINES - 12;  
    
    // text
    for(int i = 0; i < text_rows && i < editor.num_lines; i++) {
        mvprintw(text_start_row + i, 0, "%.*s", COLS, editor.lines[i]);
    }

    mvhline(LINES - 5, 0, '=', COLS);

    // footer
    mvprintw(LINES - 4, 0, " Lines: %-6d", editor.num_lines);
    mvprintw(LINES - 3, 0, " Row: %-4d Col: %-4d", editor.row + 1, editor.col + 1);
    mvprintw(LINES - 2, 0, " File: %s", editor.filename);
    mvprintw(LINES - 1, 0, " Max Line: %d", editor.max_line_length);

    // cursor position:
    int cursor_row = text_start_row + editor.row;
    if (cursor_row > LINES - 6) cursor_row = LINES - 6;
    move(cursor_row, editor.col);
    refresh();
}

void insert_char(int c) {
    if(editor.col < MAX_COLS - 1) {
        memmove(&editor.lines[editor.row][editor.col + 1],
                &editor.lines[editor.row][editor.col],
                strlen(&editor.lines[editor.row][editor.col]) + 1);
        editor.lines[editor.row][editor.col] = c;
        editor.col++;
    }
}

void handle_input(int c) {
    switch(c) {
        case KEY_UP:
            if(editor.row > 0) editor.row--;
            break;
        case KEY_DOWN:
            if(editor.row < editor.num_lines - 1) editor.row++;
            break;
        case KEY_LEFT:
            if(editor.col > 0) editor.col--;
            break;
        case KEY_RIGHT:
            if(editor.col < strlen(editor.lines[editor.row])) editor.col++;
            break;
        case KEY_BACKSPACE:
        case 127:
            if(editor.col > 0) {
                memmove(&editor.lines[editor.row][editor.col - 1],
                       &editor.lines[editor.row][editor.col],
                       strlen(&editor.lines[editor.row][editor.col]) + 1);
                editor.col--;
            }
            break;
        case '\n':
            if(editor.num_lines < MAX_LINES - 1) {
                memmove(&editor.lines[editor.row + 1],
                       &editor.lines[editor.row],
                       (MAX_LINES - editor.row - 1) * sizeof(char *));
                editor.lines[editor.row + 1] = strdup(editor.lines[editor.row] + editor.col);
                editor.lines[editor.row][editor.col] = '\0';
                editor.num_lines++;
                editor.row++;
                editor.col = 0;
            }
            break;
        case 19:  // Ctrl+S for saving
            break;
        default:
            if(c >= 32 && c <= 126) {
                insert_char(c);
            }
            break;
    }
}

int main(int argc, char *argv[]) {
    init_editor();
    init_ncurses();
    
    if(argc > 1) {
        // File loading 
    }

    int c;
    while((c = getch()) != KEY_F(1)) {
        handle_input(c);
        draw_editor();
        
        // keep cursor in text boudraries : not rly working rn
        if(editor.col > strlen(editor.lines[editor.row])) {
            editor.col = strlen(editor.lines[editor.row]);
        }
    }

    endwin();
    
    // Free memory
    for(int i = 0; i < MAX_LINES; i++) {
        free(editor.lines[i]);
    }
    free(editor.lines);
    
    return 0;
}

