#define _XOPEN_SOURCE_EXTENDED //required for wchar to be supported properly 
#include <stdio.h> // Standard I/O
#include <locale.h> // To specify that the utf-8 standard is used 
#include <wchar.h> // UTF-8, wide char hnadling
#include <stdbool.h> //Easy boolean support
#include <sys/resource.h> // Allows to query system's specific properties
#include <ncurses.h> // Primary GUI library 
#include <stdlib.h>
#include <string.h>
#include <math.h> // for ceil and other math
#include <sys/wait.h>  
#include <unistd.h>

#include "textStructure.h" // Interface to the central text datastructure 
#include "guiUtilities.h" // Some utility backend used for the GUI
#include "debugUtil.h" // For easy managmenet of logger and error messages
#include "profiler.h" //Custom profiler for easy metrics
#include "undoRedoUtilities.h" // handler for all undo/redos

#define CTRL_KEY(k) ((k) & 0x1f)

#define MENU_HEIGHT 2 //lines of menu

#define BUTTON_HEIGHT 1
#define BUTTON_SAVE_WIDTH 6
#define BUTTON_SEARCH_WIDTH 8  
#define BUTTON_SR_WIDTH 5
#define BUTTON_SPACING 2

#define FIELD_WIDTH 14
#define FIELD_PROMPT_WIDTH 8 

typedef struct {
    int x, y, width;
    char* label;
    bool pressed;
} Button;
Button buttons[3];
/*
=========================
  Text sequence data structure
=========================
*/

/*======== usage variables (of current file) ========*/
Sequence* activeSequence = NULL;
LineBstd currentLineBreakStd = NO_INIT;
LineBidentifier currentLineBidentifier = NONE_ID;

static int cursorX = 0, cursorY = 0;
static int cursorEndX = 0, cursorEndY = 0;

bool refreshFlag = true;

static int lastGuiHeight = 0, lastGuiWidth = 0;

int screenTopLine = 0;

// Menu implementations:
enum _MenuState { NOT_IN_MENU, FIND, FIND_CYCLE, F_AND_R1, F_AND_R2, F_AND_R_CYCLE };
static enum _MenuState currMenuState = NOT_IN_MENU;
static int menuCursor = 0;
#define MAX_MENU_INPUT 15
static wchar_t firstMenuInput[MAX_MENU_INPUT] = L"";// access first menue
static wchar_t secondMenuInput[MAX_MENU_INPUT] = L"";//access second menue

/*======== forward declarations ========*/
void init_editor(void);
void init_buttons(void);
void close_editor(void);
void checkSizeChanged(void);
void process_input(void);
bool is_printable_unicode(wint_t wch);
void changeScrolling(int incrY, bool enterKey);

// Regular cursor mode:
void changeAndupdateCursorAndMenu(int incrX, int incrY);
void relocateAndupdateCursorAndMenu(int newX, int newY);
// Range (more then 1 char) cursor mode:
void changeRangeEndAndUpdate(int incrX, int incrY);
void relocateRangeEndAndUpdate(int newX, int newY);
void getCurrentSelectionRang(int* rtStartX, int* rtEndX, int* rtStartY, int* rtEndY);
// State functions:
bool cursorNotInRangeSelectionState();
void resetRangeSelectionState();
// Central function to always use when updating menu (cursor functions call it on their own as well).
void updateCursorAndMenu();

// Don't use unless you know what you're doing!
void relocateCursorNoUpdate(int newX, int newY);

// horizontal scrolling auto handler:
bool autoAdjustHorizontalScrolling(bool forEndCursor);

// Helper functions:
ReturnCode deleteCurrentSelectionRange();


/*======== operations ========*/
ReturnCode open_and_setup_file(char* filePath, LineBstd stdIfNewCreation){
    
    activeSequence = loadOrCreateNewFile(filePath, stdIfNewCreation);
    if(activeSequence == NULL){
        ERR_PRINT("Sequence pointer ended as NULL in initial setup, Fatal error.\n");
    }
    
    currentLineBreakStd = getCurrentLineBstd();
    currentLineBidentifier = getCurrentLineBidentifier();
    return 1;
}

/*
=========================
  GUI
=========================
*/

/* Prints at most the requested number of following lines including the utf-8 char at "firstAtomic". Return code 1: single block accessed; code 2: multiple blocks accessed */
ReturnCode print_items_after(Position firstAtomic, int nbrOfLines){
    DEBG_PRINT("[Trace] : in print function\n");
    if ( activeSequence == NULL || currentLineBreakStd == NO_INIT || currentLineBidentifier == NONE_ID ){
        ERR_PRINT("Internal state not ready for printing... bool '1' if issue at: %d>linbStd, %d>lineBident, %d>sequence\n\n", currentLineBreakStd == NO_INIT, currentLineBidentifier == NONE_ID, activeSequence == NULL);
        return -1;
    }
    int currLineBcount = 0;
    bool requestNextBlock = false;
    int size = -1;

    //In order to ensure porting line variables for if split over multiple blocks:
    int atomicsInLine = 0; // not an index! (+1 generally) 
    int lastHorizScroll = -1;
    int sinceHorizScrollCounter = 0;
    int nbrOfUtf8CharsInLine = 0;
    int nbrOfUtf8CharsNoControlCharsInLine = 0; // If we want to ignore line breaks.
    int frozenLineStart = firstAtomic; // Stays set until line break or end of text for statistics

    while( currLineBcount < nbrOfLines ){
        //DEBG_PRINT("[Trace] : in main print while loop, %p %d %d \n", activeSequence, currentLineBreakStd, currentLineBidentifier);
        Atomic* currentItemBlock = NULL;
        if(requestNextBlock){
            //DEBG_PRINT("[Trace] : Consecutive block requested\n");
            firstAtomic = firstAtomic + size; // since size == last index +1 no additional +1 needed.
            size = (int) getItemBlock(activeSequence, firstAtomic, &currentItemBlock);
            //DEBG_PRINT("The size value %d\n", size);
            if (size < 0){
                return 2;
            }
            requestNextBlock = false;
        } else{
            //DEBG_PRINT("[Trace] : First block requested\n");
            size = (int) getItemBlock(activeSequence, firstAtomic, &currentItemBlock);
            //DEBG_PRINT("The size value %d\n", size);
        }
        

        if(( size <= 0 ) || ( currentItemBlock == NULL )){DEBG_PRINT("Main error: size value %d\n", size); return -1; }//Error!!

        int currentSectionStart = 0; //i.e. offset of nbr of Items form pointer start
        int offsetCounter = 0; //i.e. RUNNING offset of nbr of Items form currentSectionStart
        int nbrOfUtf8Chars = 0;
        int nbrOfUtf8CharsNoControlChars = 0;// If we want to ignore line breaks etc.

        while((currLineBcount < nbrOfLines) && !requestNextBlock){
            //DEBG_PRINT("handling atomic at index: %d, is : '%c' \n", (firstAtomic + currentSectionStart + offsetCounter), currentItemBlock[currentSectionStart + offsetCounter]);

            if( (currentItemBlock[currentSectionStart + offsetCounter] & 0xC0) != 0x80 ){
                // adds +1, if current atomic not == 10xxxxxx (see utf-8 specs):
                nbrOfUtf8Chars++; 

                if(currentItemBlock[currentSectionStart + offsetCounter] >= 0x20){
                    // Increase if not an (ASCII) control character:
                    nbrOfUtf8CharsNoControlChars++; 
                } 
            }
            if((currentItemBlock[currentSectionStart + offsetCounter] == currentLineBidentifier) || ((currentSectionStart+ offsetCounter) == size-1)){
                //DEBG_PRINT("found a line (or at the end of block)! current line count = %d \n", (currLineBcount +1));
                //DEBG_PRINT("Number of UTF-8 chars in this line/end of block = %d \n",  nbrOfUtf8Chars);
                
                if( ((currentLineBreakStd == MSDOS) && ((currentSectionStart + offsetCounter) > 0) && (currentItemBlock[currentSectionStart + offsetCounter-1] != '\r'))){ // Might remove if it causes issues, MSDOS should also work if only check for '\n' characters ('\r' then simply not evaluated).
                    /* Error case, lonely '\n' found despite '\r\n' current standard !*/
                    ERR_PRINT("Warning case, lonely '\n' found despite \"\r\n\" current standard !\n");
                }
                wchar_t* lineToPrint = utf8_to_wchar(&currentItemBlock[currentSectionStart], offsetCounter+1, nbrOfUtf8Chars);
                if (lineToPrint == NULL){
                    ERR_PRINT("utf_8 to Wchar conversion failed! ending here!\n");
                    return -1;
                }
                /*
                #ifdef DEBUG
                // Basic test print to test backend:
                DEBG_PRINT("section start = %d, Offset = %d \n", currentSectionStart, offsetCounter);
                char* textContent = (char*)currentItemBlock;
                DEBG_PRINT("~~~~~~~~~~~~~~~~~~\n");
                for (size_t i = 0; lineToPrint[i] != L'\0'; i++) {
                    if(lineToPrint[i] != L'\n'){
                        DEBG_PRINT("%lc", (uint32_t) lineToPrint[i]);
                    } else {
                        DEBG_PRINT("[\\n]");
                    }
                }
                DEBG_PRINT("\n~~~~~~~~~~~~~~~~~~\n");
                for (int i = currentSectionStart; i <= currentSectionStart + offsetCounter; i++) {
                    DEBG_PRINT("| %02x |", (uint8_t) textContent[i]);
                }
                DEBG_PRINT("\n~~~~~~~~~~~~~~~~~~\n");
                #endif  
                */
                if(currentItemBlock[currentSectionStart] != END_OF_TEXT_CHAR){
                    //print out line or block (could be either!!), interpreted as UTF-8 sequence:atomicsInLine:
                    DEBG_PRINT(">>>>>>Trying to print: line %d, at column %d\n", currLineBcount, nbrOfUtf8CharsNoControlCharsInLine);
                    // Horizontal scrolling implementation:
                    int horizontalScroll = getCurrHorizontalScrollOffset();
                    // always cut off string at largest size... 
                    if ((nbrOfUtf8CharsNoControlCharsInLine - horizontalScroll + nbrOfUtf8Chars) > lastGuiWidth-1){
                        int calc = lastGuiWidth -1 + horizontalScroll -nbrOfUtf8CharsInLine;
                        if(calc > 0 && calc < wcslen(lineToPrint)){
                            lineToPrint[calc] = L'\0';
                            DEBG_PRINT("Cutoff the block at:%d\n", calc);
                        } else{
                            lineToPrint[0] = L'\0';
                            DEBG_PRINT("Cutoff the block at:0\n");
                        }
                        
                    }

                    // Counter so that print knows where to print depending on horizontal scroll.
                    if(lastHorizScroll != horizontalScroll){
                        lastHorizScroll = horizontalScroll;
                        sinceHorizScrollCounter = 0;
                        DEBG_PRINT("horiz scroll change registered: %d\n", horizontalScroll);
                    }
                    if (nbrOfUtf8CharsNoControlCharsInLine + nbrOfUtf8CharsNoControlChars > horizontalScroll){ 
                        mvwaddwstr(stdscr, currLineBcount, sinceHorizScrollCounter, lineToPrint + horizontalScroll);
                        DEBG_PRINT("Printing line/block %ls\n", lineToPrint);
                        mvwaddwstr(stdscr, currLineBcount, nbrOfUtf8CharsNoControlCharsInLine, lineToPrint + horizontalScroll);
                    } else {
                        DEBG_PRINT("skipping print due to horiz scroll:  %d < %d + %d\n", horizontalScroll, nbrOfUtf8CharsNoControlCharsInLine, nbrOfUtf8CharsNoControlChars);
                    }

                }

                /* reset&setup for next block/line iteration: */
                free(lineToPrint);
                if ((currentItemBlock[currentSectionStart + offsetCounter] == currentLineBidentifier) || (currentItemBlock[currentSectionStart + offsetCounter] == END_OF_TEXT_CHAR)){
                    // handle this blocks line stats:
                    atomicsInLine += offsetCounter+1;
                    nbrOfUtf8CharsInLine += nbrOfUtf8Chars;
                    nbrOfUtf8CharsNoControlCharsInLine += nbrOfUtf8CharsNoControlChars;

                    // Save in the line stats (from variables) into dedicated management structure:
                    updateLine(currLineBcount, frozenLineStart, nbrOfUtf8CharsNoControlCharsInLine);


                    if (sinceHorizScrollCounter == 0){ 
                        DEBG_PRINT("Printed empty");
                        mvwaddwstr(stdscr, currLineBcount, 0, L"");
                    }

                    currLineBcount++;

                    //DEBG_PRINT("atomicsInLine: %d, nbrOfUtf8CharsInLine: %d, nbrOfUtf8CharsNoControlCharsInLine: %d\n", atomicsInLine, nbrOfUtf8CharsInLine, nbrOfUtf8CharsNoControlCharsInLine);
                    // Reset variables for next line:
                    atomicsInLine = 0;
                    sinceHorizScrollCounter = 0;
                    frozenLineStart = firstAtomic + currentSectionStart + offsetCounter + 1;//
                    nbrOfUtf8CharsInLine = 0;
                    nbrOfUtf8CharsNoControlCharsInLine = 0;
                } else{
                    // Ensure port of line statistics to next block handling (iteration):
                    atomicsInLine += offsetCounter+1;
                    nbrOfUtf8CharsInLine += nbrOfUtf8Chars;
                    sinceHorizScrollCounter += nbrOfUtf8CharsNoControlChars;
                    nbrOfUtf8CharsNoControlCharsInLine += nbrOfUtf8CharsNoControlChars;
                    //DEBG_PRINT("Ported, current state: atomicsInLine: %d, nbrOfUtf8CharsInLine: %d, nbrOfUtf8CharsNoControlCharsInLine: %d\n", atomicsInLine, nbrOfUtf8CharsInLine, nbrOfUtf8CharsNoControlCharsInLine);
                }
                currentSectionStart = currentSectionStart + offsetCounter + 1;
                offsetCounter = -1;
                nbrOfUtf8Chars = 0;
                nbrOfUtf8CharsNoControlChars = 0;
            }
            offsetCounter++;
            if((currentSectionStart + offsetCounter >= size)){
                //DEBG_PRINT("Setting flag for consecutive request\n");
                requestNextBlock = true;
            }
        }   
	}
    return 1;
}

/*
=========================
  Main implementation
=========================
*/
int main(int argc, char *argv[]){


    //Setup debugger utility:
    if (0 > initDebuggerFiles()){
        //Error case, but ignore since messages already sent...
    }
    DEBG_PRINT("initialized!\n");

    // Set UTF-8 locale
    if(setlocale(LC_ALL, "en_US.UTF-8") == NULL){ 
        ERR_PRINT("Fatal error: failed to set LOCALE to UTF-8!\n");
        return 1;
    } 
    DEBG_PRINT("Wide char type: %d\n",sizeof(wchar_t));
    DEBG_PRINT("Wide int char type: %d\n",sizeof(wint_t));

    LineBstd toUseForNew = NO_INIT;
    DEBG_PRINT("ARG count%d\n", argc);
    if(argc > 1){
        if(argc > 2){
            DEBG_PRINT("handling line b std arg input.\n");
            switch (atoi(argv[2])){
                case 0:
                    toUseForNew = LINUX; 
                    DEBG_PRINT("Linux case.\n");
                    break;
                case 1:
                    toUseForNew = MSDOS; 
                    DEBG_PRINT("Msdos case.\n");
                    break;
                case 2:
                    DEBG_PRINT("Mac case.\n");
                    toUseForNew = MAC; 
                    break;
            }
        }
    } else{
        ERR_PRINT("Error argc insufficient.\n");
        fprintf(stderr, "Argument issue, usage: ./textterminal.out [mandatory relative path to existing or new file] [only for new file creation: file standard 0,1, or 2]\n");
        exit(-1);
    }

    if (open_and_setup_file(argv[1],toUseForNew) < 0) {
        ERR_PRINT("Failed to create empty sequence!\n");
        close_editor();
        exit(-1);
    }

    // Initialize ncurses first
    init_editor();

    //insert(activeSequence, 0, L"Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad");
    
    // Get initial screen size
    getmaxyx(stdscr, lastGuiHeight, lastGuiWidth);
    
    //insert(activeSequence, 0, L"Test my truncation.");
    // Main editor loop
    while (1) {
    checkSizeChanged();
    process_input();
    
    if(refreshFlag){
        erase(); // Clear screen to prevent artifacts
        
        profilerStart();
        // Render text if we have valid dimensions
        if (activeSequence != NULL && lastGuiHeight > MENU_HEIGHT) {
            int linesToRender = lastGuiHeight - MENU_HEIGHT;
            if (linesToRender > 0) {
                DEBG_PRINT("Refreshing text now, from atomic %d.\n", getPrintingPortAtomicPosition());
                print_items_after(getPrintingPortAtomicPosition(), linesToRender);
            }
        }
        // Draws stats and position cursor back where it should be:
        updateCursorAndMenu();
        
        refresh();
        refreshFlag = false;
        profilerStop("gui refresh");
    }
}

    close_editor();
    return 0;
}

void init_editor(void) {
    if (!initscr()) {
        fprintf(stderr, "Error: failed to initialize ncurses\n");
        exit(EXIT_FAILURE);
    }
    
    raw();                    // Disable line buffering
    noecho();                 // Don't echo keys to screen
    keypad(stdscr, TRUE);     // Enable function keys
    
    // Get initial screen size
    getmaxyx(stdscr, lastGuiHeight, lastGuiWidth);
    mousemask(ALL_MOUSE_EVENTS, NULL);
    
    // Validate screen size
    if (lastGuiHeight < 3 || lastGuiWidth < 10) {
        endwin();
        fprintf(stderr, "Error: terminal too small (need at least 3x10)\n");
        exit(EXIT_FAILURE);
    }
    init_buttons();

    // Initialize line statistics for first (initial) iteration
    updateLine(0, 0, 0);
    refreshFlag = true;
}

void init_buttons() {
    int start_x = 2; // Start buttons 2 characters from left edge
    
    // Save button
    buttons[0].x = start_x;
    buttons[0].width = BUTTON_SAVE_WIDTH;
    buttons[0].label = "Save";
    buttons[0].pressed = false;
    
    // Search button  
    buttons[1].x = start_x + BUTTON_SAVE_WIDTH + BUTTON_SPACING;
    buttons[1].width = BUTTON_SEARCH_WIDTH;
    buttons[1].label = "Search";
    buttons[1].pressed = false;
    
    // S&R button
    buttons[2].x = start_x + BUTTON_SAVE_WIDTH + BUTTON_SPACING + BUTTON_SEARCH_WIDTH + BUTTON_SPACING;
    buttons[2].width = BUTTON_SR_WIDTH;
    buttons[2].label = "S&R";
    buttons[2].pressed = false;
}

// Draw buttons
void draw_buttons() {
    if (lastGuiHeight < MENU_HEIGHT) return;
    
    for (int i = 0; i < 3; i++) {
        // Set button appearance based on pressed state
        if (buttons[i].pressed) {
            attron(A_REVERSE); // Inverted colors for pressed state
        }
        
        // Draw button background
        mvprintw(lastGuiHeight-1, buttons[i].x, "[%s]", buttons[i].label);
        
        if (buttons[i].pressed) {
            attroff(A_REVERSE);
        }
    }
}

void handle_button_press(int button_index) {
    switch (button_index) {
        case 0: // Save button
            DEBG_PRINT("Save button pressed\n");
            // TODO: Implement save functionality
            // You'll need to add file saving logic here
            if (lastGuiHeight >= MENU_HEIGHT) {
                mvprintw(lastGuiHeight - 1, buttons[2].x + buttons[2].width + 10, "File saved!");
                refresh();
            }
            break;
            
        case 1: // Search button
            DEBG_PRINT("Search button pressed\n");
            currMenuState = FIND;
            menuCursor = 0;
            // Clear the search input
            wmemset(firstMenuInput, L'\0', MAX_MENU_INPUT);
            refreshFlag = true;
            break;
            
        case 2: // S&R (Search & Replace) button
            DEBG_PRINT("S&R button pressed\n");
            currMenuState = F_AND_R1;
            menuCursor = 0;
            // Clear both inputs
            wmemset(firstMenuInput, L'\0', MAX_MENU_INPUT);
            wmemset(secondMenuInput, L'\0', MAX_MENU_INPUT);
            refreshFlag = true;
            break;
    }
}

void draw_text_input_field(int y, int x, int width, const wchar_t* prompt, const wchar_t* input, int cursor_pos, bool active) {
    // Clear the area
    mvprintw(y, x, "%*s", width + FIELD_PROMPT_WIDTH + 3, "");
    
    // Draw prompt
    mvprintw(y, x, "%ls: ", prompt);
    int prompt_len = wcslen(prompt) + 2; 
    
    // Draw input box brackets
    mvprintw(y, x + prompt_len, "[");
    mvprintw(y, x + prompt_len + width + 1, "]");
    
    // Draw input text
    if (wcslen(input) > 0) {
        // Convert wide string to multibyte for printing
        size_t input_len = wcslen(input);
        char* mb_input = malloc((input_len * 4 + 1) * sizeof(char));
        if (mb_input) {
            size_t converted = wcstombs(mb_input, input, input_len * 4);
            if (converted != (size_t)-1) {
                mb_input[converted] = '\0';
                // Limit display to field width
                int display_len = (strlen(mb_input) > width) ? width : strlen(mb_input);
                mvprintw(y, x + prompt_len + 1, "%.*s", display_len, mb_input);
            }
            free(mb_input);
        }
    }
    
    // Position cursor if this field is active
    if (active) {
        int cursor_screen_pos = cursor_pos;
        if (cursor_screen_pos > width) cursor_screen_pos = width;
        move(y, x + prompt_len + 1 + cursor_screen_pos);
    }
}

void draw_menu_interface() {
    DEBG_PRINT("Drawing menu interface: state=%d\n", currMenuState);
    if (currMenuState == NOT_IN_MENU) return;
    
    int menu_y = lastGuiHeight - 1;  // Same line as buttons
    int field_start_x = buttons[2].x + buttons[2].width + 10;  // After S&R button
    
    switch (currMenuState) {
        case FIND:
        case FIND_CYCLE:
            // Clear area after buttons
            mvprintw(menu_y, field_start_x, "%*s", lastGuiWidth - field_start_x, "");
            draw_text_input_field(menu_y, field_start_x, FIELD_WIDTH, L"Search", firstMenuInput, menuCursor, true);
            // Optional: Add instruction text if there's space
            int instr_x = field_start_x + FIELD_WIDTH + FIELD_PROMPT_WIDTH + 5;
            if (instr_x < lastGuiWidth - 20) {
                mvprintw(menu_y, instr_x, "Enter to search, Esc to cancel");
            }
            break;
            
        case F_AND_R1:
            // Clear area after buttons
            mvprintw(menu_y, field_start_x, "%*s", lastGuiWidth - field_start_x, "");
            draw_text_input_field(menu_y, field_start_x, FIELD_WIDTH, L"Find", firstMenuInput, menuCursor, true);
            int instr_x1 = field_start_x + FIELD_WIDTH + FIELD_PROMPT_WIDTH + 5;
            if (instr_x1 < lastGuiWidth - 15) {
                mvprintw(menu_y, instr_x1, "Enter to continue");
            }
            break;
            
        case F_AND_R2:
        case F_AND_R_CYCLE:
            // Use two lines for find and replace
            mvprintw(menu_y - 1, field_start_x, "%*s", lastGuiWidth - field_start_x, "");
            mvprintw(menu_y, field_start_x, "%*s", lastGuiWidth - field_start_x, "");
            
            // Draw both input fields
            draw_text_input_field(menu_y - 1, field_start_x, FIELD_WIDTH, L"Find", firstMenuInput, 0, false);
            draw_text_input_field(menu_y, field_start_x, FIELD_WIDTH, L"Replace", secondMenuInput, menuCursor, true);
            
            int instr_x2 = field_start_x + FIELD_WIDTH + FIELD_PROMPT_WIDTH + 5;
            if (instr_x2 < lastGuiWidth - 25) {
                mvprintw(menu_y, instr_x2, "Enter to replace, Esc to cancel");
            }
            break;
    }
}

int check_button_click(int mouse_x, int mouse_y) {
    for (int i = 0; i < 3; i++) {
        if (mouse_y == lastGuiHeight-1 && 
            mouse_x >= buttons[i].x && 
            mouse_x <= buttons[i].x + buttons[i].width + 1) { // +1 for brackets
            return i;
        }
    }
    return -1;
}


void handle_menu_input(wint_t wch, int status) {
    DEBG_PRINT("Menu input: state=%d, wch=%d\n", currMenuState, wch);
    if (currMenuState == NOT_IN_MENU) return;
    
    if (status == KEY_CODE_YES) {
        switch (wch) {
            case KEY_LEFT:
                if (menuCursor > 0) {
                    menuCursor--;
                    refreshFlag = true;
                }
                break;
                
            case KEY_RIGHT:
                if (currMenuState == FIND || currMenuState == F_AND_R1) {
                    if (menuCursor < (int)wcslen(firstMenuInput)) {
                        menuCursor++;
                        refreshFlag = true;
                    }
                } else if (currMenuState == F_AND_R2) {
                    if (menuCursor < (int)wcslen(secondMenuInput)) {
                        menuCursor++;
                        refreshFlag = true;
                    }
                }
                break;
                
            case KEY_BACKSPACE:
            case 8:
            case 127:  // Also handle DEL key as backspace in menu
                if (menuCursor > 0) {
                    if (currMenuState == FIND || currMenuState == F_AND_R1) {
                        // Remove character from firstMenuInput
                        wmemmove(&firstMenuInput[menuCursor-1], &firstMenuInput[menuCursor], 
                                wcslen(firstMenuInput) - menuCursor + 1);
                    } else if (currMenuState == F_AND_R2) {
                        // Remove character from secondMenuInput
                        wmemmove(&secondMenuInput[menuCursor-1], &secondMenuInput[menuCursor], 
                                wcslen(secondMenuInput) - menuCursor + 1);
                    }
                    menuCursor--;
                    refreshFlag = true;
                }
                break;
        }
    } else if (status == OK) {
        switch (wch) {
            case 27: // Escape key
                currMenuState = NOT_IN_MENU;
                refreshFlag = true;
                break;
                
            case KEY_ENTER:
            case 10:
            case 13:
                if (currMenuState == FIND) {
                    // TODO: Implement search functionality using firstMenuInput
                    DEBG_PRINT("Searching for: %ls\n", firstMenuInput);
                    currMenuState = NOT_IN_MENU;
                    refreshFlag = true;
                } else if (currMenuState == F_AND_R1) {
                    currMenuState = F_AND_R2;
                    menuCursor = 0;
                    refreshFlag = true;
                } else if (currMenuState == F_AND_R2) {
                    // TODO: Implement find and replace functionality
                    DEBG_PRINT("Find: %ls, Replace: %ls\n", firstMenuInput, secondMenuInput);
                    currMenuState = NOT_IN_MENU;
                    refreshFlag = true;
                }
                break;
                
            default:
                // Handle regular character input
                if (is_printable_unicode(wch)) {
                    wchar_t* target_input = NULL;
                    int max_len = MAX_MENU_INPUT - 1;
                    
                    if (currMenuState == FIND || currMenuState == F_AND_R1) {
                        target_input = firstMenuInput;
                    } else if (currMenuState == F_AND_R2) {
                        target_input = secondMenuInput;
                    }
                    
                    if (target_input && (int)wcslen(target_input) < max_len) {
                        // Insert character at cursor position
                        wmemmove(&target_input[menuCursor+1], &target_input[menuCursor], 
                                wcslen(target_input) - menuCursor + 1);
                        target_input[menuCursor] = (wchar_t)wch;
                        menuCursor++;
                        refreshFlag = true;
                    }
                }
                break;
        }
    }
}

//paste
ReturnCode pasteFromClipboard(Sequence* sequence, int cursorY, int cursorX) {
    if (sequence == NULL) {
        ERR_PRINT("Invalid sequence for paste operation\n");
        return -1;
    }
    
    char* clipboardText = getFromXclip();
    if (clipboardText == NULL) {
        ERR_PRINT("Failed to get text from clipboard\n");
        return -1;
    }
    
    // Convert UTF-8 to wide characters
    size_t wcharCount = mbstowcs(NULL, clipboardText, 0);
    if (wcharCount == (size_t)-1) {
        ERR_PRINT("Invalid UTF-8 in clipboard text\n");
        free(clipboardText);
        return -1;
    }
    
    wchar_t* wideText = malloc((wcharCount + 1) * sizeof(wchar_t));
    if (wideText == NULL) {
        ERR_PRINT("Failed to allocate memory for wide character conversion\n");
        free(clipboardText);
        return -1;
    }
    
    mbstowcs(wideText, clipboardText, wcharCount + 1);
    free(clipboardText);
    
    // Insert the text at cursor position using getAbsoluteAtomicIndex
    int insertPos = getAbsoluteAtomicIndex(cursorY, cursorX, sequence);
    if (insertPos < 0) {
        ERR_PRINT("Failed to calculate insertion position\n");
        free(wideText);
        return -1;
    }

    DEBG_PRINT("Pasting at position: Y=%d, X=%d\n", cursorY, cursorX);
    ReturnCode result = insert(sequence, insertPos, wideText);
    free(wideText);
    
    return result;
}

/**
 * Get text from xclip
 */
char* getFromXclip(void) {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        ERR_PRINT("Failed to create pipe for xclip read\n");
        return NULL;
    }
    
    pid_t pid = fork();
    if (pid == -1) {
        ERR_PRINT("Failed to fork for xclip read\n");
        close(pipefd[0]);
        close(pipefd[1]);
        return NULL;
    }
    
    if (pid == 0) {
        // Child - run xclip
        close(pipefd[0]); 
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        
        execl("/usr/bin/xclip", "xclip", "-selection", "clipboard", "-o", NULL);
        _exit(1); 
    } else {
        // Parent - read pipe
        close(pipefd[1]); 
        
        char* buffer = malloc(4096);
        if (buffer == NULL) {
            ERR_PRINT("Failed to allocate buffer for clipboard read\n");
            close(pipefd[0]);
            waitpid(pid, NULL, 0);
            return NULL;
        }
        
        ssize_t totalRead = 0;
        ssize_t bufferSize = 4096;
        ssize_t bytesRead;
        
        while ((bytesRead = read(pipefd[0], buffer + totalRead, bufferSize - totalRead - 1)) > 0) {
            totalRead += bytesRead;
            
            if (totalRead >= bufferSize - 1) {
                bufferSize *= 2;
                char* newBuffer = realloc(buffer, bufferSize);
                if (newBuffer == NULL) {
                    ERR_PRINT("Failed to reallocate buffer for clipboard read\n");
                    free(buffer);
                    close(pipefd[0]);
                    waitpid(pid, NULL, 0);
                    return NULL;
                }
                buffer = newBuffer;
            }
        }
        
        close(pipefd[0]);
        
        int status;
        waitpid(pid, &status, 0);
        
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            ERR_PRINT("xclip paste operation failed\n");
            free(buffer);
            return NULL;
        }
        
        buffer[totalRead] = '\0';
        return buffer;
    }
}
int copyYoffset = 0;
int copyXoffset = 0;
ReturnCode copySelectionToClipboard(Sequence* sequence) {
    if (sequence == NULL) {
        ERR_PRINT("Invalid sequence for copy operation\n");
        return -1;
    }
    
    // Get current selection range using the function from first file
    int startX, endX, startY, endY;
    getCurrentSelectionRang(&startX, &endX, &startY, &endY);
    
    copyYoffset = endY-startY;
    copyXoffset = endX-startX;

    // Use getAbsoluteAtomicIndex to get positions
    int startPos = getAbsoluteAtomicIndex(startY, startX, sequence);
    int endPos = getAbsoluteAtomicIndex(endY, endX, sequence)-1;
    
    if (startPos < 0 || endPos < 0) {
        ERR_PRINT("Failed to calculate absolute positions for copy\n");
        return -1;
    }
    
    // Ensure proper order
    if (startPos > endPos) {
        int temp = startPos;
        startPos = endPos;
        endPos = temp;
    }
    
    wchar_t* copiedText = extractTextRange(sequence, startPos, endPos);
    if (copiedText == NULL) {
        ERR_PRINT("Failed to extract text for copy\n");
        return -1;
    }
    
    size_t utf8Size = wcstombs(NULL, copiedText, 0);
    if (utf8Size == (size_t)-1) {
        ERR_PRINT("Failed to calculate UTF-8 size for copy\n");
        free(copiedText);
        return -1;
    }
    
    char* utf8Text = malloc(utf8Size + 1);
    if (utf8Text == NULL) {
        ERR_PRINT("Failed to allocate memory for UTF-8 conversion\n");
        free(copiedText);
        return -1;
    }
    
    DEBG_PRINT("Copying selection: startY=%d, startX=%d, endY=%d, endX=%d\n", startY, startX, endY, endX);
    DEBG_PRINT("Copying selection Atomic: startPos=%d, endPos=%d\n", startPos, endPos);
    wcstombs(utf8Text, copiedText, utf8Size + 1);
    free(copiedText);
    
    // Send to xclip
    ReturnCode result = sendToXclip(utf8Text);
    free(utf8Text);
    
    return result;
}

/**
 * Send text to xclip for clipboard storage
 */
ReturnCode sendToXclip(const char* text) {
    if (text == NULL) {
        ERR_PRINT("No text provided to sendToXclip\n");
        return -1;
    }
    
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        ERR_PRINT("Failed to create pipe for xclip write\n");
        return -1;
    }
    
    pid_t pid = fork();
    if (pid == -1) {
        ERR_PRINT("Failed to fork for xclip write\n");
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }
    
    if (pid == 0) {
        // Child - run xclip
        close(pipefd[1]);
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        
        execl("/usr/bin/xclip", "xclip", "-selection", "clipboard", "-i", NULL);
        _exit(1);
    } else {
        // Parent - write to pipe
        close(pipefd[0]);
        
        size_t textLen = strlen(text);
        ssize_t written = write(pipefd[1], text, textLen);
        close(pipefd[1]);
        
        if (written != (ssize_t)textLen) {
            ERR_PRINT("Failed to write complete text to xclip\n");
            waitpid(pid, NULL, 0);
            return -1;
        }
        
        int status;
        waitpid(pid, &status, 0);
        
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            ERR_PRINT("xclip copy operation failed\n");
            return -1;
        }
        
        return 1;
    }
}

/**
 * Extract text between abs positions
 * Returns allocated wchar_t string must be freed
 */
wchar_t* extractTextRange(Sequence* sequence, int startPos, int endPos) {
    if (sequence == NULL || startPos < 0 || endPos < startPos) {
        return NULL;
    }
    
    // Calculate total length needed
    int totalLength = endPos - startPos + 1;
    int utf8CharCount = 0;
    int currentPos = startPos;
    
    // Count UTF-8 characters in range
    while (currentPos <= endPos) {
        Atomic* block = NULL;
        Size blockSize = getItemBlock(sequence, currentPos, &block);
        if (blockSize <= 0 || block == NULL) {
            break;
        }
        
        Size remainingInRange = endPos - currentPos + 1;
        Size toProcess = (blockSize < remainingInRange) ? blockSize : remainingInRange;
        
        for (int i = 0; i < toProcess; i++) {
            if ((block[i] & 0xC0) != 0x80) {
                utf8CharCount++;
            }
        }
        
        currentPos += toProcess;
    }
    
    if (utf8CharCount == 0) {
        return NULL;
    }
    
    // Allocate buffer for wide characters
    wchar_t* result = malloc((utf8CharCount + 1) * sizeof(wchar_t));
    if (result == NULL) {
        ERR_PRINT("Failed to allocate memory for text extraction\n");
        return NULL;
    }
    
    // Extract actual text
    currentPos = startPos;
    int resultPos = 0;
    
    while (currentPos <= endPos && resultPos < utf8CharCount) {
        Atomic* block = NULL;
        Size blockSize = getItemBlock(sequence, currentPos, &block);
        if (blockSize <= 0 || block == NULL) {
            break;
        }
        
        Size remainingInRange = endPos - currentPos + 1;
        Size toProcess = (blockSize < remainingInRange) ? blockSize : remainingInRange;
        
        // Convert this block to wide char
        mbstate_t state = {0};
        size_t atomicIdx = 0;
        
        while (atomicIdx < toProcess && resultPos < utf8CharCount) {
            size_t parseLen = mbrtowc(&result[resultPos], (const char*)&block[atomicIdx], 
                                      toProcess - atomicIdx, &state);
            
            if (parseLen == (size_t)-1) {
                result[resultPos] = L'\uFFFD'; 
                atomicIdx++;
            } else if (parseLen == (size_t)-2) {
                break; // Incomplete char
            } else {
                atomicIdx += parseLen;
            }
            resultPos++;
        }
        
        currentPos += toProcess;
    }
    
    result[resultPos] = L'\0';
    return result;
}


void close_editor(void) {
    closeDebuggerFiles;
    endwin();
    
    closeSequence(activeSequence, true); // Force close
    activeSequence = NULL;
}

void checkSizeChanged(void){
    int new_y, new_x;
    getmaxyx(stdscr, new_y, new_x);

    if (is_term_resized(lastGuiHeight, lastGuiWidth)){
        erase();
        resizeterm(new_y, new_x);
        lastGuiHeight = new_y;
        lastGuiWidth = new_x;
        
        init_buttons();


        // Validate cursor position after resize
        if (cursorY >= lastGuiHeight - MENU_HEIGHT) {
            cursorY = lastGuiHeight - MENU_HEIGHT - 1;
            if (cursorY < 0) cursorY = 0;
        }
        if (cursorX >= lastGuiWidth) {
            cursorX = lastGuiWidth - 1;
            if (cursorX < 0) cursorX = 0;
        }
        
        refreshFlag = true;
    }
}

void process_input(void) {
    wint_t wch;
    int status;

    status = get_wch(&wch);
    if (currMenuState != NOT_IN_MENU) {
        handle_menu_input(wch, status);
        return;  // Don't process other input while in menu mode
    }
    // Handle Ctrl+l properly
    if (status == OK && wch == CTRL_KEY('l')) {// changed to 'l' since issue with VS code not allowing ctrl + q inputs.
        close_editor();
        exit(0);
    }
    // ctrl+p for paste 
     if (status == OK && wch == CTRL_KEY('p')) {
        DEBG_PRINT("Processing Ctrl+P (paste)\n");
        
        int originalCursorX = cursorX;
        int originalCursorY = cursorY;
        
        if (pasteFromClipboard(activeSequence, cursorY, cursorX) > 0) {
            DEBG_PRINT("Paste successful\n");
            
            if (copyYoffset == 0) {//single line
                cursorX += copyXoffset;
            } else {// Multi-line 
                
                cursorY += copyYoffset;
                cursorX = copyXoffset;
            }
            
            // Validate cursor bounds
            if (cursorX < 0) cursorX = 0;
            if (cursorY < 0) cursorY = 0;
            if (cursorY >= lastGuiHeight - MENU_HEIGHT) {
                cursorY = lastGuiHeight - MENU_HEIGHT - 1;
            }
            if (cursorX >= lastGuiWidth) {
                cursorX = lastGuiWidth - 1;
            }
            
            resetRangeSelectionState();
            refreshFlag = true;
            setLineStatsNotUpdated();
            
            if (lastGuiHeight >= MENU_HEIGHT) {
                mvprintw(lastGuiHeight - 2, 0, "Text pasted   Ctrl-l to quit");
                refresh();
            }
        } else {
            ERR_PRINT("Failed to paste from clipboard\n");
            if (lastGuiHeight >= MENU_HEIGHT) {
                mvprintw(lastGuiHeight - 2, 0, "Paste failed   Ctrl-l to quit");
                refresh();
            }
        }
    }

    // ctrl+y for copy
if (status == OK && wch == CTRL_KEY('y')) {
    DEBG_PRINT("Processing Ctrl+Y (copy)\n");
    if (copySelectionToClipboard(activeSequence) > 0) {
        DEBG_PRINT("Copy successful\n");
        
        if (lastGuiHeight >= MENU_HEIGHT) {
            mvprintw(lastGuiHeight - 2, 0, "Text copied   Ctrl-l to quit ");
            refresh();
        }
    } else {
        ERR_PRINT("Failed to copy to clipboard\n");
        if (lastGuiHeight >= MENU_HEIGHT) {
            mvprintw(lastGuiHeight - 2, 0, "Copy failed   Ctrl-l to quit ");
            refresh();
        }
    }
}

    if (status == OK && wch ==  CTRL_KEY('z')){
        DEBG_PRINT("Processing UNDO.\n");
        if(undo(activeSequence) > 0){
            DEBG_PRINT("Undo might have succeeded.\n");
            refreshFlag = true;
        }
    }

    if (status == OK && wch == CTRL_KEY('r')){
        DEBG_PRINT("Processing REDO.\n");
        if (redo(activeSequence) > 0){
            DEBG_PRINT("Redo might have succeeded.\n");
            refreshFlag = true;
        }
    }

    if (status == OK && wch == CTRL_KEY('s')){
        DEBG_PRINT("Processing SAVE.\n");
        if (saveSequence(activeSequence) > 0){
            DEBG_PRINT("Save might have succeeded.\n");
        } else{
            // handling?
        }
    }

    if (status == KEY_CODE_YES) {
        // Function key pressed
        int posStart = -1; // Used for delete and backspace
        int posEnd = -1; // Used for delete and backspace
        switch (wch){
            case KEY_MOUSE:
                MEVENT event;
                if (getmouse(&event) == OK) {
                    DEBG_PRINT("Handling MOUSE event.\n");
                    if (event.bstate & BUTTON1_CLICKED) {
                        // Check if click is on a button first
                        int button_clicked = check_button_click(event.x, event.y);
                        if (button_clicked != -1) {
                            // Button was clicked
                            buttons[button_clicked].pressed = true;
                            draw_buttons();
                            refresh();
                            
                            // Small delay to show button press
                            napms(100);
                            
                            buttons[button_clicked].pressed = false;
                            handle_button_press(button_clicked);
                            button_clicked = -1;
                            updateCursorAndMenu();
                        }
                        else if (event.y < lastGuiHeight - MENU_HEIGHT) {
                            // Text area click
                            currMenuState = NOT_IN_MENU;
                            // Text cursor case:
                            relocateAndupdateCursorAndMenu(event.x, event.y);
                        }  else{
                            // Menu interactions case:

                        }
                    } 
                    else if (event.bstate & BUTTON1_DOUBLE_CLICKED) {
                        DEBG_PRINT("Drag ended at: X:%d Y:%d", event.x, event.y);
                        relocateRangeEndAndUpdate(event.x, event.y);
                    }
                }
                break;
            /*---- Standard Cursor ----*/
            case KEY_UP:
                DEBG_PRINT("[CURSOR]:UP\n");
                changeAndupdateCursorAndMenu(0,-1);
                break;
                
            case KEY_DOWN:
                DEBG_PRINT("[CURSOR]: DOWN\n");
                changeAndupdateCursorAndMenu(0,1);
                break;
                
            case KEY_LEFT:
                DEBG_PRINT("[CURSOR]: LEFT\n");
                changeAndupdateCursorAndMenu(-1,0);
                break;
                
            case KEY_RIGHT: 
                DEBG_PRINT("[CURSOR]: RIGHT\n");
                if(currMenuState != NOT_IN_MENU){
                    if(menuCursor+1 < MAX_MENU_INPUT){
                       //menuCursor++; Disable for now
                    } else{
                        //menuCursor = MAX_MENU_INPUT-1;
                    }
                    updateCursorAndMenu();
                    break;
                }
                //Otherwise standard text cursor:
                changeAndupdateCursorAndMenu(1,0);              
                break;
             /*---- Range Cursor ----*/
            case KEY_SRIGHT: // Shift right arrow
                changeRangeEndAndUpdate(1,0);
                break;
            case KEY_SLEFT: // Shift left arrow
                changeRangeEndAndUpdate(-1,0);
                break;
            case KEY_NPAGE: // Page up as select up
                //changeRangeEndAndUpdate(0,1);
                changeScrolling(1, false);
                break;
            case KEY_PPAGE: // Page down as select down
                //changeRangeEndAndUpdate(0,-1);
                changeScrolling(-1, false);
                break;
             /*---- Backspace & Delete ----*/
            case KEY_BACKSPACE: // Backspace
            case 8:
                DEBG_PRINT("Processing 'BACKSPACE'\n");


                // Calculation with range support:
                if (cursorNotInRangeSelectionState()){
                    if((cursorY > 0) && (cursorX == 0)){
                        // Case of at begining of a line:
                        if(!(getCurrentLineBstd() == MSDOS)){
                            DEBG_PRINT("Backspace remove '\n' case...\n");
                            posStart = getAbsoluteAtomicIndex(cursorY,0, activeSequence)-1;
                            DEBG_PRINT("Pos would have been: %d but now %d",posStart +1, posStart);
                            posEnd = posStart;
                        } else{
                            //special MSDOS handling:
                            DEBG_PRINT("Backspace remove '\r\n' case...\n");
                            posStart = getAbsoluteAtomicIndex(cursorY,0, activeSequence)-2;
                            DEBG_PRINT("(MSDOS); Pos would have been: %d but now %d",posStart +2, posStart);
                            posEnd = posStart+1;
                        }
                        //debugPrintInternalState(activeSequence, true, false);

                        // so that automatically placed at previous line end:
                        relocateCursorNoUpdate(-1, cursorY);
                    } else if (!((cursorX == 0) && (cursorY == 0))){
                        //all other valid cases:
                        DEBG_PRINT("Backspace standard case...\n");
                        posStart = getAbsoluteAtomicIndex(cursorY, cursorX -1, activeSequence);
                        // Ensure multibyte case:
                        posEnd = getAbsoluteAtomicIndex(cursorY, cursorX, activeSequence)-1;
                        // Reposition cursor:
                        relocateCursorNoUpdate(cursorX-1, cursorY);
                    } else{
                        DEBG_PRINT("BACKSPACE invalid case...\n");
                        break;
                    }

                    DEBG_PRINT("Backspace with atomics: %d to %d\n", posStart, posEnd);
                    if(delete(activeSequence, posStart, posEnd) < 0) {
                        ERR_PRINT("Backspace failed...\n");
                        break;
                    }

                } else{
                    DEBG_PRINT("Backspace with: X:%d to %d; Y:%d to %d\n", cursorX, cursorEndX, cursorY, cursorEndY);
                    if (deleteCurrentSelectionRange() < 0){
                        ERR_PRINT("Aborted BACKSPACE due to range delete fail.\n");
                        break;
                    }
                }

                refreshFlag = true;
                setLineStatsNotUpdated();
                break;

            case KEY_DC:// Delete (Backspace but for single char mirrored behavior).
            case 127:
                DEBG_PRINT("Processing 'DELETE'\n");

                // Calculation with range support:
                if (cursorNotInRangeSelectionState()){
                    if((cursorY + 1 < getTotalAmountOfRelativeLines()) && (cursorX == getUtfNoControlCharCount(cursorY))){
                        if(!(getCurrentLineBstd() == MSDOS)){
                            // Case of at end of a line:
                            posStart = getAbsoluteAtomicIndex(cursorY + 1, 0, activeSequence)-1;
                            posEnd = posStart;
                            DEBG_PRINT("Pos would have been: %d but now %d",posStart +1, posStart);
                            //debugPrintInternalState(activeSequence, true, false);
                        } else{
                            //special MSDOS handling:
                            posStart = getAbsoluteAtomicIndex(cursorY + 1, 0, activeSequence)-2;
                            posEnd = posStart+1;
                            DEBG_PRINT("(MSDOS); Pos would have been: %d but now %d",posStart +2, posStart);
                            //debugPrintInternalState(activeSequence, true, false);
                        }

                        // No cursor change/relocate needed due to behavior of DELETE.
                    } else if (!((cursorY == getTotalAmountOfRelativeLines() -1) && (cursorX == getUtfNoControlCharCount(cursorY)))){
                            //all other valid cases:
                            DEBG_PRINT("Delete standard case...\n");
                            posStart = getAbsoluteAtomicIndex(cursorY, cursorX, activeSequence);
                            // Ensure multi byte support:
                            posEnd = getAbsoluteAtomicIndex(cursorY, cursorX+1, activeSequence)-1;
                    } else{
                        DEBG_PRINT("DELETE invalid case...\n");
                        break;
                    }
                    DEBG_PRINT("Delete with atomics: %d to %d\n", posStart, posStart);
                    if(delete(activeSequence, posStart, posEnd) < 0) {
                        ERR_PRINT("Delete failed...\n");
                        break;
                    }

                } else{
                    DEBG_PRINT("Delete with: X:%d to %d; Y:%d to %d\n", cursorX, cursorEndX, cursorY, cursorEndY);
                    if (deleteCurrentSelectionRange() < 0){
                        ERR_PRINT("Aborted DELETE due to range delete fail.\n");
                        break;
                    }
                }

                refreshFlag = true;
                setLineStatsNotUpdated();
                break;
            
            default:
                DEBG_PRINT("Ignored character input: U+%04X (decimal: %d), name %s\n", (unsigned int)wch, (int)wch, keyname(wch));
                break;
        }
    } 
    else if (status == OK) {
        switch (wch) {
            case KEY_ENTER: // Enter key
            case 10:
            case 13:
                if(currMenuState != NOT_IN_MENU){
                    DEBG_PRINT("Handling enter in Menu Mode");
                    // Everything Find & Replace:
                    if(currMenuState == F_AND_R1){
                        menuCursor = 0;
                        currMenuState = F_AND_R2;
                    } else if (currMenuState == F_AND_R2 || currMenuState == F_AND_R_CYCLE){
                        //TODO launch F&R procedure via future TextStructure.h interface here with cursorX+1/cursorY values.
                        jumpAbsoluteLineNumber(0,0); // TODO
                        cursorX = 0;// TODO
                        cursorY = 0;// TODO
                        refreshFlag = true;
                        break;
                    }
                    updateCursorAndMenu();
                    break;
                }
                DEBG_PRINT("Enter pressed at cursor position (%d, %d)\n", cursorY, cursorX);

                if(deleteCurrentSelectionRange() < 0){
                    ERR_PRINT("Aborted ENTER insert due to range delete fail.\n");
                    break;
                }
                // Set here already since at least delete succeeded: 
                
                int atomicPos = getAbsoluteAtomicIndex(cursorY, cursorX, activeSequence);
                DEBG_PRINT("Calculated atomic position: %d\n", atomicPos);
                
                if (atomicPos >= 0) {
                    wchar_t toInsert[3];  // Buffer to hold the line ending

                    switch (getCurrentLineBstd()) {
                        case LINUX:
                            wcscpy(toInsert, L"\n");
                            break;
                        case MSDOS:
                            wcscpy(toInsert, L"\r\n");
                            break;
                        case MAC:
                            wcscpy(toInsert, L"\r");
                            break;
                        default:
                            ERR_PRINT("Enter input could not be handled since line break std not properly initialized.\n");
                            wcscpy(toInsert, L"");
                    }
                    
                    DEBG_PRINT("Inserting line break at atomic position %d\n", atomicPos);

                    if (insert(activeSequence, atomicPos, toInsert) > 0) {
                        // Exceptionally set it without safety in order to allow for leap of faith... 
                        cursorX = 0;
                        cursorY++;
                        resetRangeSelectionState();
                        //changeScrolling(1, true);
                        DEBG_PRINT("After Enter: cursor moved to (%d, %d)\n", cursorY, cursorX);
                    } else {
                        ERR_PRINT("Failed to insert line break\n");
                        }
                } else {
                    ERR_PRINT("Failed to get atomic position for Enter key\n");
                }
                refreshFlag = true;
                setLineStatsNotUpdated();
                break;

            // Regular character input 
            default:
                if (is_printable_unicode(wch)) {
                    DEBG_PRINT("STANDARD INSERT:\n");
                    // Ensure section state is correctly handled first:
                    if(deleteCurrentSelectionRange() < 0){
                        ERR_PRINT("Aborted INSERT to range delete fail.\n");
                        break;
                    }

                    // Get position for insertion
                    int atomicPos = getAbsoluteAtomicIndex(cursorY, cursorX, activeSequence);
                    if (atomicPos >= 0) {
                        // Convert single character to null-terminated wide string
                        
                        DEBG_PRINT("Inserting Unicode character: U+%04X '%lc' at position %d\n", (unsigned int)wch, wch, atomicPos);
                        DEBG_PRINT("The character width: %d", wcwidth(wch));
                        wchar_t convertedWchar[2];  // Wide string to hold the character + null terminator
                        convertedWchar[0] = (wchar_t)wch;
                        convertedWchar[1] = L'\0';
                        

                        if (insert(activeSequence, atomicPos, convertedWchar) > 0) {
                            // Exceptionally set it without safety in order to allow for leap of faith... 
                            cursorX++;
                            resetRangeSelectionState();   
                        }
                    } else {
                        DEBG_PRINT("Invalid atomic position for insert: %d\n", atomicPos);
                    }
                    refreshFlag = true;
                    setLineStatsNotUpdated();
                    //debugPrintInternalState(activeSequence, true, true);
                }// Log unhandled case for debugging
                else {
                    DEBG_PRINT("Ignored character input: U+%04X (decimal: %d), name %s\n", (unsigned int)wch, (int)wch, keyname(wch));
                }
                break;
        }
    }
}


/*
=================
  Cursor handling
=================
*/

// Handle vertical scrolling
void changeScrolling(int incrY, bool enterKey){
    int newY = cursorY + incrY;

    if (incrY != 0) {
        DEBG_PRINT("changeScrolling in if statement (incrY != 0)\n");
        int totalLines = getCurrentLineCount(activeSequence);
        DEBG_PRINT("totalLines: %d\n", totalLines);
        if (totalLines < 0) totalLines = 0;  // Handle error case if totalLines negative
        
        int visibleLines = lastGuiHeight - MENU_HEIGHT; // Lines on screen
        if (visibleLines < 0) {
            // Not enough space for text display
            newY = 0;
        } else {
            if (incrY < 0) {
                // Scroll up
                DEBG_PRINT("changeScrolling scroll up\n");
                if (screenTopLine > 0) { // Can't scroll up further when at the very top
                    screenTopLine--;
                    cursorY++;
                    cursorEndY++;
                    moveAbsoluteLineNumbers(activeSequence, -1);
                    newY = 0;
                    refreshFlag = true;
                } else {
                    newY = 0;
                }
            } else if (incrY > 0 && enterKey == false) {
                // Scroll down
                DEBG_PRINT("changeScrolling scroll down\n");
                if (0 < visibleLines) {
                    screenTopLine++;
                    cursorY--;
                    cursorEndY--;
                    moveAbsoluteLineNumbers(activeSequence, 1);
                    newY = visibleLines - 1;
                    refreshFlag = true;
                } else {
                    newY = visibleLines - 1;
                }
            } //else if (enterKey == true && cursorY == visibleLines){
                // Scroll down
                //DEBG_PRINT("changeScrolling scroll down with enter key\n");
                //screenTopLine++;
                //moveAbsoluteLineNumbers(activeSequence, 1);
                //newY = visibleLines - 1;
                //refreshFlag = true;
            //}
        }
    }
}

/* ----- Increment cursor -----*/
/**
 * Function to easily and SAFELY increment/decrement by relative values (only +1 or -1 have good support) 
 * (e.g. incrX has the internal effect: x = x + incrX) cursorX/Y variables and update. Resets any range states.
 * Automatically jumps to next/previous line if legal to do so. 
 */
void changeAndupdateCursorAndMenu(int incrX, int incrY){
    /*Debugging*/
    //DEBG_PRINT("Atomic position of cursor calc:%d\n", getAbsoluteAtomicIndex(cursorY + incrY,cursorX + incrX, activeSequence));
    //debugPrintInternalState(activeSequence, true,false);
    /*Debugging end*/
    relocateAndupdateCursorAndMenu(cursorX + incrX, cursorY + incrY);
}

/* ----- Jump cursor -----*/
/**
 * Function to easily and SAFELY reposition (jump) to specific X/Y coordinates in the text. Resets any range states.
 * Automatically jumps to next/previous line if legal to do so.  
 */
void relocateAndupdateCursorAndMenu(int newX, int newY){
    relocateCursorNoUpdate(newX, newY);
    

    updateCursorAndMenu();
}

/**
 * Don't use me unless you know what your're doing.
 * 
 * Function to easily and SAFELY reposition (jump) to specific X/Y coordinates in the text. Resets any range states.
 * Automatically jumps to next/previous line if legal to do so.  
 */
void relocateCursorNoUpdate(int newX, int newY){
    if (currMenuState != NOT_IN_MENU){
        if(currMenuState == FIND || currMenuState == F_AND_R1){
            if(newX < wcslen(firstMenuInput) && newX >= 0){
                menuCursor = newX;
            }
        } else if(currMenuState == F_AND_R2){
            if(newX < wcslen(secondMenuInput) && newX >= 0){
                menuCursor = newX;
            }
        } else{
            //Exit menu mode since interrupted
            currMenuState = NOT_IN_MENU;
        }
        updateCursorAndMenu();
        return;
    }
    bool changedY = !(cursorY == newY);
    // Handle Y:
    int amtOfRelativeLines = getTotalAmountOfRelativeLines();
    if(newY < amtOfRelativeLines && newY >= 0){
        // Standard case (x handled in next big if block):
        DEBG_PRINT("Y standard case.\n");
        cursorY = newY;

    } else if (newY < 0){
        // Can't take negatives, just put at beginning:
        DEBG_PRINT("Y beyond start case.\n");
        cursorY = 0;
        cursorX = 0;
        resetRangeSelectionState();
        return;

    } else if (newY >= amtOfRelativeLines) {
        // Special case of beyond last line:
        DEBG_PRINT("Y beyond end case.\n");
        cursorY = amtOfRelativeLines -1;
        cursorX = getUtfNoControlCharCount(cursorY);
        resetRangeSelectionState();
        return;

    } else {
        ERR_PRINT("Unexpected, unhandled case in 'relocateCursor'\n");
        return;
    }

    // Handle X (with respect to new Y): 
    int charCountAtY = getUtfNoControlCharCount(cursorY);
    if(newX <= charCountAtY && newX >= 0){
        DEBG_PRINT("X standard case.\n");
        cursorX = newX;
    } else if (newX > charCountAtY && cursorY +1 < amtOfRelativeLines){
        // Beyond line end:
        DEBG_PRINT("X Beyond line end case.\n");
        if (!changedY){
            cursorX = 0;
            cursorY += 1;
        } else {
            cursorX = charCountAtY;
        }
    } else if (newX < 0 && cursorY -1 >= 0){
        // Go to previous since before first char of line.
        DEBG_PRINT("X Beyond line start case.\n");
        cursorY += -1;
        cursorX = getUtfNoControlCharCount(cursorY);
    } else{
        if(newX > charCountAtY){
            DEBG_PRINT("X Special line end case.\n");
            cursorX = charCountAtY;
        } else {
            DEBG_PRINT("Invalid case skipped in 'relocateCursor', but range reset & update performed.\n");
        }
    }
    resetRangeSelectionState();
}

/* ----- Change cursor selection range -----*/

/**
 * Function to easily and SAFELY increment/decrement by relative values (e.g. incrX has the internal effect: x = x + incrX) cursorX/Y variables. Resets any range states.
 * Automatically jumps to next/previous line if legal to do so. 
 */
void changeRangeEndAndUpdate(int incrX, int incrY){
    relocateRangeEndAndUpdate(cursorEndX + incrX, cursorEndY + incrY);
}

/**
 * Function to easily and SAFELY selected range.
 * Automatically jumps to next/previous line if legal to do so. 
 */
void relocateRangeEndAndUpdate(int newX, int newY){
    DEBG_PRINT("Handling cursor RANGE, trying to go to rng end: X:%d Y:%d", newX, newY);
    bool changedY = !(cursorY == newY);
    // Handle Y:
    int amtOfRelativeLines = getTotalAmountOfRelativeLines();
    if(newY < amtOfRelativeLines  && newY >= 0){
        // Standard case (x handled in next big if block):
        cursorEndY = newY;
    } else if (newY < 0){
        // Can't take negatives, just put at beginning:
        cursorEndY = 0;
        cursorEndX = 0;
        resetRangeSelectionState();
        return;

    } else if (newY >= amtOfRelativeLines) {
        // Special case of beyond last line:
        cursorEndY = amtOfRelativeLines -1;
        cursorEndX = getUtfNoControlCharCount(cursorEndY) - getCurrHorizontalScrollOffset();
        resetRangeSelectionState();
        return;

    } else {
        ERR_PRINT("Unexpected, unhandled case in 'relocateRangeEndAndUpdate()'\n");
        return;
    }

    // Handle X (with respect to new Y): 
    int charCountAtY = getUtfNoControlCharCount(cursorEndY);
    if(newX <= charCountAtY && newX >= 0){
        cursorEndX = newX;
    } else if (newX > charCountAtY && cursorEndY +1 < amtOfRelativeLines){
        // Beyond line end:
        if(!changedY){
            cursorEndX = 0;
            cursorEndY += 1;
        } else {
            cursorEndX = charCountAtY;
        }
    } else if (newX < 0 && cursorEndY -1 >= 0){
        // Go to previous since before first char of line.
        cursorEndY += -1;
        cursorEndX = getUtfNoControlCharCount(cursorEndY);
    } else{
        if(newX > charCountAtY){
            cursorEndX = charCountAtY;
        } else {
            DEBG_PRINT("Invalid case skipped in 'relocateRangeEndAndUpdate()', but update performed.\n");
        }
    }
    updateCursorAndMenu();
}


/**
 * Function to relocate horizontal scrolling seting so that cursor is in range.
 * Returns true if updated.
 */
bool autoAdjustHorizontalScrolling(bool forEndCursor){
    int currHorizScroll = getCurrHorizontalScrollOffset();
    int newHorizScroll = 0;

    int baseScroll = (int)(0.5 * lastGuiWidth);

    // hardcoded limit to move horiz Scroll by 1/2 width as soon as reach rightEnd-2 or move back as soon as lower 3. 
    if(!forEndCursor){
        int multiplier = (int)round(abs(cursorX) / (double)baseScroll);
        if(currHorizScroll > 0 && cursorX < 3){
            DEBG_PRINT("Horiz scroll case 1.\n");
            newHorizScroll = -(multiplier * baseScroll);
        } else if(cursorX > lastGuiWidth-3) {
            DEBG_PRINT("Horiz scroll case 2.\n");
            newHorizScroll = +(multiplier * baseScroll);
        }
    } else{
       int multiplier = (int)round(abs(cursorEndX) / (double)baseScroll);
        if(currHorizScroll > 0 && cursorEndX < 3){
            DEBG_PRINT("Horiz scroll case 3.\n");
            newHorizScroll = -(multiplier * baseScroll);
        } else if(cursorEndX > lastGuiWidth-3) {
            DEBG_PRINT("Horiz scroll case 4.\n");
            newHorizScroll = +(multiplier * baseScroll);
        } 
    }

    if (newHorizScroll != 0){
        DEBG_PRINT("Auto horiz scroll has found need for update: %d.\n", newHorizScroll);
        changeHorizontalScrollOffset(newHorizScroll);
        cursorX += -newHorizScroll;
        cursorEndX += -newHorizScroll;
        refreshFlag = true;
        return true;
    } else{
        DEBG_PRINT("No need for horiz scroll update.\n");
        return false;
    }
}

/**
 * Function to automatically get correctly ordered start and end values of current selection range.
 * Simply returns identical values if not in range section mode.
 */
void getCurrentSelectionRang(int* rtStartX, int* rtEndX, int* rtStartY, int* rtEndY){
    if (cursorNotInRangeSelectionState()){
        // Implicit case of not in range mode:
        *rtStartX = cursorX;
        *rtStartY = cursorY;
        *rtEndX = cursorX;
        *rtEndY = cursorY;
        return;

    } else{
        if (cursorY < cursorEndY){
            // Standard case:
            *rtStartX = cursorX;
            *rtStartY = cursorY;
            *rtEndX = cursorEndX;
            *rtEndY = cursorEndY;
            return;

        } else if (cursorY > cursorEndY){
            // Inverse end and start...
            *rtStartX = cursorEndX;
            *rtStartY = cursorEndY;
            *rtEndX = cursorX;
            *rtEndY = cursorY;
            return;

        } else {
            // Single line cases:
            if (cursorX <= cursorEndX){
                *rtStartX = cursorX;
                *rtEndX = cursorEndX;
                // Should be identical...
                *rtEndY = cursorY;
                *rtStartY = cursorY;
                return;

            } else {
                *rtStartX = cursorEndX;
                *rtEndX = cursorX;
                // Should be identical...
                *rtEndY = cursorY;
                *rtStartY = cursorY;
                return;
            }
        }
    }
}

/**
 * Updates and moves the cursor state in most efficient manner. 
 * Uses the general internal cursorX/Y (and cursorEndX/Y) variables as new position without having to pass them here.
 */
void updateCursorAndMenu(){
    // Update stats in GUI and if needed repaint the range...
    // Remove previous range display:
    for (int y = 0; y <= lastGuiHeight; y++){
        mvchgat(y, 0, -1, A_NORMAL, 0, NULL); // -1 signifies: till end of gui line
    }

    if(cursorNotInRangeSelectionState()){
        autoAdjustHorizontalScrolling(false);
        // Perform this to ensure correct ranges still ensured:
        relocateCursorNoUpdate(cursorX,cursorY);
        int horizOffs = getCurrHorizontalScrollOffset();

        if (lastGuiHeight >= MENU_HEIGHT) {
            DEBG_PRINT("Menu update1\n");
            // Clear the status line first
            move(lastGuiHeight - 1, 0);
            clrtoeol();// Clear rest of status line
            
            // Draw buttons first
            draw_buttons();
            
            mvprintw(lastGuiHeight - 2, 0, "Ln %d, Col %d || %d words, %d lines || Ctrl-l to quit", cursorY + horizOffs + 1, cursorX + horizOffs + 1, getCurrentWordCount(activeSequence), getCurrentLineCount(activeSequence));
        }
    } else {
        autoAdjustHorizontalScrolling(true);

        int startX = -1, startY = -1, endX = -1, endY = -1;
        getCurrentSelectionRang(&startX, &endX, &startY, &endY);
        if (startY == endY){
            mvchgat(endY, startX, endX-startX, A_REVERSE, 0, NULL);// Format in inverted color scheme
        } else{
            mvchgat(startY, startX, -1, A_REVERSE, 0, NULL);
            mvchgat(endY, 0, endX, A_REVERSE, 0, NULL);
            for (int y = startY+1; y <= endY -1; y++){
                mvchgat(y, 0, -1,A_REVERSE, 0, NULL);
            }
        }
        
        if (lastGuiHeight >= MENU_HEIGHT) {
            DEBG_PRINT("Menu update2\n");
            move(lastGuiHeight - 1, 0);
            clrtoeol();
            
            // Draw buttons
            draw_buttons();
            
            // Draw selection status if not in menu
            if (currMenuState == NOT_IN_MENU) {
                int horizOffs = getCurrHorizontalScrollOffset();
                int status_x = buttons[2].x + buttons[2].width + 10;
                mvprintw(lastGuiHeight - 2, 0, "Ln %d-%d, Col %d-%d || %d words, %d lines || Ctrl-l to quit", cursorY + horizOffs + 1, cursorEndY + horizOffs +1, cursorX + horizOffs + 1, cursorEndX + horizOffs +1, getCurrentWordCount(activeSequence), getCurrentLineCount(activeSequence));
            }

        }
    }
    draw_menu_interface();

    // Position cursor appropriately
    if (currMenuState != NOT_IN_MENU) {
        // Cursor positioning is handled in draw_menu_interface
    } else {
        DEBG_PRINT("Ncurses cursor moved to screen pos X:%d, Y:%d\n", cursorX, cursorY);
        move(cursorY, cursorX);
    }
    refresh();
}
/**
 * Function to know if currently in range selection mode. 
 * Use this every time range and normal selection modes need different handling.
 */
bool cursorNotInRangeSelectionState(){
    return cursorX == cursorEndX && cursorY == cursorEndY;
}

/**
 * Function to reset or update range to be equal to cursor.
 */
void resetRangeSelectionState(){
    cursorEndX = cursorX;
    cursorEndY = cursorY;
    return;
}

/**
 * Deletes what's in selection range and resets the selection state. If currently not in range state, call simply ignored.
 * Does not call cursor update since usually used in cases where general refresh is done right after anyways.
 */
ReturnCode deleteCurrentSelectionRange(){
    if(!cursorNotInRangeSelectionState()){
        int startX = -1, endX = -1, startY = -1, endY = -1;
        getCurrentSelectionRang(&startX, &endX, &startY, &endY);
        if (delete(activeSequence, getAbsoluteAtomicIndex(startY,startX,activeSequence), getAbsoluteAtomicIndex(endY,endX,activeSequence)-1) < 0 ){
            ERR_PRINT("Failed to delete what's in selection range!\n");
            return -1;
        }
        DEBG_PRINT("Deleting selection range: startX=%d, endX=%d, startY=%d, endY=%d\n", startX, endX, startY, endY);
        relocateCursorNoUpdate(startX, startY);
    }
    return 1;
}

/*
================
  Unicode Utils
================
*/

/**
 * Check if a wide character is printable (not a control character)
 * This includes all Unicode characters that should be displayed
 */
bool is_printable_unicode(wint_t wch) {
    // Basic ASCII printable range
    if (wch >= 32 && wch <= 126) {
        return true;
    }
    
    // Extended ASCII and Unicode ranges
    if (wch >= 160) { // Non-breaking space and above
        // Exclude some common control character ranges
        if ((wch >= 0x007F && wch <= 0x009F) ||  // C1 control characters
            (wch >= 0xFDD0 && wch <= 0xFDEF) ||  // Non-characters
            (wch >= 0xFFFE && wch <= 0xFFFF)) {  // Non-characters
            return false;
        }
        return true;
    }
    
    // Some additional printable characters in the lower range
    if (wch == 0x00A0 ||  // Non-breaking space
        (wch >= 0x00A1 && wch <= 0x00FF)) {  // Latin-1 supplement
        return true;
    }
    
    return false;
}