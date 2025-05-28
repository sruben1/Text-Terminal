#define _XOPEN_SOURCE_EXTENDED //required for wchar to be supported properly 
#include <stdio.h> // Standard I/O
#include <locale.h> // To specify that the utf-8 standard is used 
#include <wchar.h> // UTF-8, wide char hnadling
#include <stdbool.h> //Easy boolean support
#include <sys/resource.h> // Allows to query system's specific properties
#include <ncurses.h> // Primary GUI library 
#include <stdlib.h>
#include <string.h>

#include "textStructure.h" // Interface to the central text datastructure 
#include "guiUtilities.h" // Some utility backend used for the GUI
#include "debugUtil.h" // For easy managmenet of logger and error messages
#include "profiler.h" //Custom profiler for easy metrics

#define CTRL_KEY(k) ((k) & 0x1f)

#define MENU_HEIGHT 2 //lines of menu

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
static Position topLineAtomic = 0;
static Position lineStartAtomic[1024];
bool refreshFlag = true;

static int lastGuiHeight = 0, lastGuiWidth = 0;

/*======== forward declarations ========*/
void init_editor(void);
void close_editor(void);
void checkSizeChanged(void);
void process_input(void);
bool is_printable_unicode(wint_t wch);

/*======== operations ========*/
ReturnCode open_and_setup_file(char* file_path){
    // Don't reassign activeSequence if it's already initialized
    if (activeSequence == NULL) {
        activeSequence = empty(); // Remove parameter if empty() takes none
        if (activeSequence == NULL) {
            ERR_PRINT("Failed to create empty sequence in open_and_setup_file!\n");
            return -1;
        }
    }
    currentLineBreakStd = LINUX;
    currentLineBidentifier = LINUX_MSDOS_ID;
    return 1;
}

/*
=========================
  GUI
=========================
*/

/* Fixed and improved version of print_items_after */
ReturnCode print_items_after(Position firstAtomic, int nbrOfLines){
    DEBG_PRINT("[Trace] : in print function\n");
    
    // Critical safety checks
    if (activeSequence == NULL) {
        ERR_PRINT("activeSequence is NULL in print_items_after\n");
        return -1;
    }
    if (currentLineBreakStd == NO_INIT) {
        ERR_PRINT("currentLineBreakStd not initialized\n");
        return -1;
    }
    if (currentLineBidentifier == NONE_ID) {
        ERR_PRINT("currentLineBidentifier not initialized\n");
        return -1;
    }
    if (nbrOfLines <= 0) {
        DEBG_PRINT("No lines to print (nbrOfLines = %d)\n", nbrOfLines);
        return 1;
    }
    
    int currLineBcount = 0;
    Position currentPos = firstAtomic;

    while(currLineBcount < nbrOfLines){
        // Prevent array overflow in lineStats
        if (currLineBcount >= 75) {
            DEBG_PRINT("Reached maximum lines limit (75), stopping render\n");
            break;
        }
        
        DEBG_PRINT("[Trace] : processing line %d at position %d\n", currLineBcount, (int)currentPos);
        
        Atomic* currentItemBlock = NULL;
        Size size = getItemBlock(activeSequence, currentPos, &currentItemBlock);
        
        if(size <= 0 || currentItemBlock == NULL){
            // Check if we've reached end of text
            if (currentItemBlock != NULL && *currentItemBlock == END_OF_TEXT_CHAR) {
                DEBG_PRINT("Reached end of text\n");
                // Update line stats for empty line and break
                updateLine(currLineBcount, currentPos, 0);
                break;
            }
            DEBG_PRINT("No more blocks available or invalid block\n");
            break;
        }

        // Store the start position of this line
        Position lineStartPos = currentPos;
        
        // Process current line from current position
        int lineLength = 0;
        int utf8CharCount = 0;
        int displayableCharCount = 0;
        Position scanPos = currentPos;
        bool foundLineEnd = false;
        
        // Scan through blocks to find the complete line
        while(!foundLineEnd) {
            Atomic* scanBlock = NULL;
            Size scanSize = getItemBlock(activeSequence, scanPos, &scanBlock);
            
            if(scanSize <= 0 || scanBlock == NULL) {
                break;
            }
            
            // Scan this block for the line end
            for(int i = 0; i < scanSize; i++) {
                Atomic currentChar = scanBlock[i];
                
                // Check for end of text
                if (currentChar == END_OF_TEXT_CHAR) {
                    foundLineEnd = true;
                    break;
                }
                
                // Count UTF-8 characters
                if((currentChar & 0xC0) != 0x80){
                    utf8CharCount++;
                    if(currentChar >= 0x20){
                        displayableCharCount++; 
                    }
                }
                
                lineLength++;
                
                // Check for line break
                if(currentChar == currentLineBidentifier || currentChar == '\n'){
                    foundLineEnd = true;
                    break;
                }
            }
            
            if(!foundLineEnd) {
                scanPos += scanSize;
            }
        }
        
        // Convert line to wide characters for display
        if (lineLength > 0) {
            // We need to collect the line data from potentially multiple blocks
            char* lineData = malloc(lineLength);
            if(lineData == NULL) {
                ERR_PRINT("Failed to allocate memory for line data\n");
                break;
            }
            
            // Collect line data
            Position collectPos = currentPos;
            int collected = 0;
            
            while(collected < lineLength) {
                Atomic* collectBlock = NULL;
                Size collectSize = getItemBlock(activeSequence, collectPos, &collectBlock);
                
                if(collectSize <= 0 || collectBlock == NULL) {
                    break;
                }
                
                int toCopy = (lineLength - collected < collectSize) ? lineLength - collected : collectSize;
                memcpy(lineData + collected, collectBlock, toCopy);
                collected += toCopy;
                collectPos += toCopy;
            }
            
            wchar_t* lineToPrint = utf8_to_wchar((Atomic*)lineData, lineLength, utf8CharCount);
            free(lineData);
            
            if (lineToPrint != NULL) {
                // Actually render the line to screen
                int displayRow = currLineBcount;
                if (displayRow < lastGuiHeight - MENU_HEIGHT) {
                    mvwaddwstr(stdscr, displayRow, 0, lineToPrint);
                    // Clear rest of line to handle shorter lines
                    clrtoeol();
                }

                #ifdef DEBUG
                DEBG_PRINT("Line %d: start_pos=%d, length=%d, utf8chars=%d, displayable=%d\n", 
                          currLineBcount, (int)lineStartPos, lineLength, utf8CharCount, displayableCharCount);
                DEBG_PRINT("~~~~~~~~~~~~~~~~~~\n");
                for (size_t i = 0; lineToPrint[i] != L'\0'; i++) {
                    if(lineToPrint[i] != L'\n'){
                        DEBG_PRINT("%lc", (uint32_t) lineToPrint[i]);
                    } else {
                        DEBG_PRINT("[\\n]");
                    }
                }
                DEBG_PRINT("\n~~~~~~~~~~~~~~~~~~\n");
                #endif

                free(lineToPrint);
            }
        } else {
            // Empty line - just clear the screen line
            if (currLineBcount < lastGuiHeight - MENU_HEIGHT) {
                move(currLineBcount, 0);
                clrtoeol();
            }
        }

        // Update line statistics with correct values
        updateLine(currLineBcount, lineStartPos, displayableCharCount);
        
        // Move to next line (skip the line break character if present)
        currentPos = lineStartPos + lineLength;
        currLineBcount++;
    }
    
    // Clear any remaining lines on screen
    for(int i = currLineBcount; i < lastGuiHeight - MENU_HEIGHT; i++) {
        move(i, 0);
        clrtoeol();
    }
    
    return 1;
}

/*
=========================
  Unicode Helper Functions
=========================
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

/*
=========================
  Main implementation
=========================
*/
int main(int argc, char *argv[]){
    DEBG_PRINT("initialized!\n");
    
    // Set UTF-8 locale
    if(setlocale(LC_ALL, "en_US.UTF-8") == NULL){ 
        ERR_PRINT("Fatal error: failed to set LOCALE to UTF-8!\n");
        return 1;
    } 
    
    // Initialize ncurses first
    init_editor();
    
    // Initialize the text sequence
    activeSequence = empty();
    if (activeSequence == NULL) {
        ERR_PRINT("Failed to create empty sequence!\n");
        close_editor();
        return 1;
    }
    
    // Set up text processing parameters
    currentLineBreakStd = LINUX;
    currentLineBidentifier = LINUX_MSDOS_ID;
    
    // Initialize line statistics
    setLineStatsNotUpdated();
    topLineAtomic = 0;
    
    // Initialize line statistics for empty document
    updateLine(0, 0, 0);  // Line 0, at atomic position 0, with 0 characters
    
    // Get initial screen size
    getmaxyx(stdscr, lastGuiHeight, lastGuiWidth);
    
    // Main editor loop
    while (1) {
        checkSizeChanged();
        process_input();
        
        if(refreshFlag){
            clear(); // Clear screen to prevent artifacts
            
            // Render text if we have valid dimensions
            if (activeSequence != NULL && lastGuiHeight > MENU_HEIGHT) {
                int linesToRender = lastGuiHeight - MENU_HEIGHT;
                if (linesToRender > 0) {
                    print_items_after(topLineAtomic, linesToRender);
                }
            }
            
            // Draw status line
            if (lastGuiHeight >= MENU_HEIGHT) {
                mvprintw(lastGuiHeight - 1, 0, "Ln %d, Col %d   Ctrl-Q to quit", cursorY + 1, cursorX + 1);
                clrtoeol(); // Clear rest of status line
            }
            
            // Position cursor back where it should be
            move(cursorY, cursorX);
            
            refresh();
            refreshFlag = false;
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
    
    // Validate screen size
    if (lastGuiHeight < 3 || lastGuiWidth < 10) {
        endwin();
        fprintf(stderr, "Error: terminal too small (need at least 3x10)\n");
        exit(EXIT_FAILURE);
    }
    
    // Initialize first line stats
    updateLine(0, 0, 0);
}

void close_editor(void) {
    endwin();
    if (activeSequence) {
        closeSequence(activeSequence, true); // Force close
        activeSequence = NULL;
    }
}

void checkSizeChanged(void){
    int new_y, new_x;
    getmaxyx(stdscr, new_y, new_x);

    if (is_term_resized(lastGuiHeight, lastGuiWidth)){
        resizeterm(new_y, new_x);
        lastGuiHeight = new_y;
        lastGuiWidth = new_x;
        
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
    
    // Handle Ctrl+Q properly
    if (status == OK && wch == CTRL_KEY('q')) {
        close_editor();
        exit(0);
    }
        
    if (status == KEY_CODE_YES) {
        // Function key pressed
        switch (wch){
            case KEY_UP:
                if (cursorY > 0) {
                    cursorY--;
                    refreshFlag = true;
                }
                break;
                
            case KEY_DOWN:
                if (cursorY < lastGuiHeight - MENU_HEIGHT - 1) {
                    cursorY++;
                    refreshFlag = true;
                }
                break;
                
            case KEY_LEFT:
                if (cursorX > 0) {
                    cursorX--;
                    refreshFlag = true;
                }
                break;
                
            case KEY_RIGHT: {
                int maxCol = getUtfNoControlCharCount(cursorY);
                if (maxCol < 0) maxCol = 0;
                if (cursorX < maxCol && cursorX < lastGuiWidth - 1) {
                    cursorX++;
                    refreshFlag = true;
                }
                break;
            }
            
            default:
                break;
        }
    } 
    else if (status == OK) {
        // Regular character input 
        if (is_printable_unicode(wch) && activeSequence != NULL) {
            // Get position for insertion
            int atomicPos = getAbsoluteAtomicIndex(cursorY, cursorX, activeSequence);
            if (atomicPos >= 0) {
                // Convert single character to null-terminated wide string
                wchar_t textToInsert[2] = {wch, L'\0'};
                
                DEBG_PRINT("Inserting Unicode character: U+%04X (%lc) at position %d\n", 
                          (unsigned int)wch, wch, atomicPos);
                
                if (insert(activeSequence, atomicPos, textToInsert) > 0) {
                    cursorX++;
                    refreshFlag = true;
                    
                    // Invalidate line stats since text changed
                    setLineStatsNotUpdated();
                    // Re-initialize line stats starting from line 0
                    updateLine(0, 0, 0);
                }
            } else {
                DEBG_PRINT("Invalid atomic position for insert: %d\n", atomicPos);
            }
        }
        else if (wch == 10 || wch == 13) { // Enter key
            if (activeSequence != NULL) {
                int atomicPos = getAbsoluteAtomicIndex(cursorY, cursorX, activeSequence);
                if (atomicPos >= 0) {
                    wchar_t newline[2] = {L'\n', L'\0'};
                    if (insert(activeSequence, atomicPos, newline) > 0) {
                        cursorY++;
                        cursorX = 0;
                        refreshFlag = true;
                        
                        // Invalidate line stats
                        setLineStatsNotUpdated();
                        updateLine(0, 0, 0);
                    }
                }
            }
        }
        else if (wch == 127 || wch == 8) { // Backspace
            if (activeSequence != NULL && (cursorX > 0 || cursorY > 0)) {
                int atomicPos;
                if (cursorX > 0) {
                    atomicPos = getAbsoluteAtomicIndex(cursorY, cursorX - 1, activeSequence);
                    if (atomicPos >= 0) {
                        if (delete(activeSequence, atomicPos, atomicPos) > 0) { // Fixed: delete single character
                            cursorX--;
                            refreshFlag = true;
                            setLineStatsNotUpdated();
                            updateLine(0, 0, 0);
                        }
                    }
                } else if (cursorY > 0) {
                    // Handle backspace at beginning of line
                    int prevLineLength = getUtfNoControlCharCount(cursorY - 1);
                    atomicPos = getAbsoluteAtomicIndex(cursorY, 0, activeSequence);
                    if (atomicPos > 0) {
                        if (delete(activeSequence, atomicPos - 1, atomicPos - 1) > 0) { // Fixed: delete single character
                            cursorY--;
                            cursorX = (prevLineLength > 0) ? prevLineLength : 0;
                            refreshFlag = true;
                            setLineStatsNotUpdated();
                            updateLine(0, 0, 0);
                        }
                    }
                }
            }
        }
        else {
            // Log unhandled characters for debugging
            DEBG_PRINT("Unhandled character input: U+%04X (decimal: %d)\n", 
                      (unsigned int)wch, (int)wch);
        }
    }
}