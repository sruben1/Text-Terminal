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
static int cursorEndX = 0, cursorEndY = 0;
static Position topLineGeneralNbr = 0;
bool refreshFlag = true;

static int lastGuiHeight = 0, lastGuiWidth = 0;

/*======== forward declarations ========*/
void init_editor(void);
void close_editor(void);
void checkSizeChanged(void);
void process_input(void);
void changeAndUpdateCursor();
void updateCursor();
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
    currentLineBreakStd = LINUX;//getCurrentLineBstd();
    currentLineBidentifier = LINUX_MSDOS_ID;//getCurrentLineBidentifier();
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
    int nbrOfUtf8CharsInLine = 0;
    int nbrOfUtf8CharsNoControlCharsInLine = 0; // If we want to ignore line breaks.
    int frozenLineStart = firstAtomic; // Stays set until line break or end of text for statistics

    while( currLineBcount < nbrOfLines ){
        DEBG_PRINT("[Trace] : in main print while loop, %p %d %d \n", activeSequence, currentLineBreakStd, currentLineBidentifier);
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
                DEBG_PRINT("found a line (or at the end of block)! current line count = %d \n", (currLineBcount +1));
                DEBG_PRINT("Number of UTF-8 chars in this line/end of block = %d \n",  nbrOfUtf8Chars);
                
                if( ((currentLineBreakStd == MSDOS) && ((currentSectionStart + offsetCounter) > 0) && (currentItemBlock[currentSectionStart + offsetCounter-1] != '\r'))){ // Might remove if it causes issues, MSDOS should also work if only check for '\n' characters ('\r' then simply not evaluated).
                    /* Error case, lonely '\n' found despite '\r\n' current standard !*/
                    ERR_PRINT("Warning case, lonely '\n' found despite \"\r\n\" current standard !\n");
                }
                wchar_t* lineToPrint = utf8_to_wchar(&currentItemBlock[currentSectionStart], offsetCounter+1, nbrOfUtf8Chars);
                if (lineToPrint == NULL){
                    ERR_PRINT("utf_8 to Wchar conversion failed! ending here!\n");
                    return -1;
                }
                
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
                
                if(currentItemBlock[currentSectionStart] != END_OF_TEXT_CHAR){
                    //print out line or block (could be either!!), interpreted as UTF-8 sequence:atomicsInLine:
                    DEBG_PRINT(">>>>>>Trying to print: line %d, at column %d\n", currLineBcount, nbrOfUtf8CharsNoControlCharsInLine);
                    mvwaddwstr(stdscr, currLineBcount, nbrOfUtf8CharsNoControlCharsInLine, lineToPrint);
                    // TODO: Horizontal scrolling  (i.e.left truncation) and right side truncation needed here as well.

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
                    //Set following invalid so that last line is always identifiable as such...
                    updateLine(currLineBcount + 1, -1, 0);

                    currLineBcount++;

                    //DEBG_PRINT("atomicsInLine: %d, nbrOfUtf8CharsInLine: %d, nbrOfUtf8CharsNoControlCharsInLine: %d\n", atomicsInLine, nbrOfUtf8CharsInLine, nbrOfUtf8CharsNoControlCharsInLine);
                    // Reset variables for next line:
                    atomicsInLine = 0;
                    frozenLineStart = firstAtomic + offsetCounter + 1;
                    nbrOfUtf8CharsInLine = 0;
                    nbrOfUtf8CharsNoControlCharsInLine = 0;
                } else{
                    // Ensure port of line statistics to next block handling (iteration):
                    atomicsInLine += offsetCounter+1;
                    nbrOfUtf8CharsInLine += nbrOfUtf8Chars;
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
    DEBG_PRINT("initialized!\n");

    //Setup debugger utility:
    if (0 > initDebuggerFiles()){
        //Error case, but ignore since messages already sent...
    }
    
    // Set UTF-8 locale
    if(setlocale(LC_ALL, "en_US.UTF-8") == NULL){ 
        ERR_PRINT("Fatal error: failed to set LOCALE to UTF-8!\n");
        return 1;
    } 

    // Initialize ncurses first
    init_editor();

    if (open_and_setup_file("TODO later") < 0) {
        ERR_PRINT("Failed to create empty sequence!\n");
        close_editor();
        return 1;
    }
    
    // Initialize line statistics for first (initial) iteration
    updateLine(0, 0, 0);  // Line 0, at atomic position 0, with 0 characters
    
    // Get initial screen size
    getmaxyx(stdscr, lastGuiHeight, lastGuiWidth);
    
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
                print_items_after(topLineGeneralNbr, linesToRender);
            }
        }
        
        // Draw status line
        if (lastGuiHeight >= MENU_HEIGHT) {
            mvprintw(lastGuiHeight - 1, 0, "Ln %d, Col %d   Ctrl-l to quit", cursorY + 1, cursorX + 1);
            //clrtoeol(); // Clear rest of status line
        }
        
        // Position cursor back where it should be
        move(cursorY, cursorX);
        
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
    
    // Validate screen size
    if (lastGuiHeight < 3 || lastGuiWidth < 10) {
        endwin();
        fprintf(stderr, "Error: terminal too small (need at least 3x10)\n");
        exit(EXIT_FAILURE);
    }
    
    // Initialize first line stats
    updateLine(0, 0, 0);
    refreshFlag = true;
}

void close_editor(void) {
    closeDebuggerFiles;
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
        erase();
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
    
    // Handle Ctrl+l properly
    if (status == OK && wch == CTRL_KEY('l')) {// changed to 'l' since issue with VS code not allowing ctrl + q inputs.
        close_editor();
        exit(0);
    }
        
    if (status == KEY_CODE_YES) {
        // Function key pressed
        int start = -1;
        int end = -1;
        switch (wch){
            case KEY_UP:
                DEBG_PRINT("[CURSOR]:UP\n");
                changeAndUpdateCursor(0,-1);
                break;
                
            case KEY_DOWN:
                DEBG_PRINT("[CURSOR]: DOWN\n");
                changeAndUpdateCursor(0,1);
                break;
                
            case KEY_LEFT:
                DEBG_PRINT("[CURSOR]: LEFT\n");
                changeAndUpdateCursor(-1,0);
                break;
                
            case KEY_RIGHT: 
                DEBG_PRINT("[CURSOR]: RIGHT\n");
                changeAndUpdateCursor(1,0);              
                break;
            case KEY_BACKSPACE: // Backspace
            case 8:
                DEBG_PRINT("Processing 'BACKSPACE'\n");

                // Calculation with range support:
                if (cursorX == cursorEndX && cursorY == cursorEndY){
                    if((cursorY > 0) && (cursorX == 0)){
                        // Case of at begining of a line:
                        start = getAbsoluteAtomicIndex(cursorY - 1, getUtfNoControlCharCount(cursorY -1) -1, activeSequence);
                    } else if (!((cursorX == 0) && (cursorY == 0))){
                        //all other valid cases:
                        DEBG_PRINT("Backspace standard case...\n");
                        start = getAbsoluteAtomicIndex(cursorY, cursorX -1, activeSequence);
                    }  else{
                        DEBG_PRINT("BACKSPACE invalid case...\n");
                        break;
                    }
                    end = start;
                } else{
                    // TODO: Range case
                }

                DEBG_PRINT("cursor with: X:%d to %d; Y:%d to %d\n", cursorX, cursorEndX, cursorY, cursorEndY);
                DEBG_PRINT("Backspace with atomics: %d to %d\n", start, end);
                if(delete(activeSequence, start, end) < 0) {
                    ERR_PRINT("Backspace failed...\n");
                    break;
                }

                changeCursor(-1,0);
                refreshFlag = true;
                setLineStatsNotUpdated();
                break;

            case KEY_DC:// Delete (Backspace but for single char mirrored behavior).
            case 127:
                DEBG_PRINT("Processing 'DELETE'\n");

                // Calculation with range support:
                if (cursorX == cursorEndX && cursorY == cursorEndY){
                    if((cursorY + 1 < getTotalAmountOfRelativeLines()) && (cursorX == getUtfNoControlCharCount(cursorY))){
                        // Case of at end of a line:
                        start = getAbsoluteAtomicIndex(cursorY + 1, 0, activeSequence);
                    } else if (!((cursorY == getTotalAmountOfRelativeLines() -1) && (cursorX == getUtfNoControlCharCount(cursorY)))){
                        //all other valid cases:
                        start = getAbsoluteAtomicIndex(cursorY, cursorX, activeSequence);
                    } else{
                        DEBG_PRINT("DELETE invalid case...\n");
                        break;
                    }
                    end = start;
                } else{
                    // TODO: Range case (identical to above)
                }

                DEBG_PRINT("cursor with: X:%d to %d; Y:%d to %d\n", cursorX, cursorEndX, cursorY, cursorEndY);
                DEBG_PRINT("Delete with atomics: %d to %d\n", start, end);
                if(delete(activeSequence, start, end) < 0) {
                    ERR_PRINT("Delete failed...\n");
                    break;
                }

                refreshFlag = true;
                setLineStatsNotUpdated();
                break;
            
            default:
                break;
        }
    } 
    else if (status == OK) {
        switch (wch) {
            case KEY_ENTER: // Enter key
            case 10:
            case 13:
                DEBG_PRINT("Enter pressed at cursor position (%d, %d)\n", cursorY, cursorX);

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
                        // Move cursor to the beginning of the new line
                        cursorY++;
                        cursorX = 0;
                        refreshFlag = true;
                        
                            // Invalidate line stats
                        setLineStatsNotUpdated();
                        
                        DEBG_PRINT("After Enter: cursor moved to (%d, %d)\n", cursorY, cursorX);
                    } else {
                        ERR_PRINT("Failed to insert line break\n");
                        }
                } else {
                    ERR_PRINT("Failed to get atomic position for Enter key\n");
                }
                break;
            // Regular character input 
            default:
                if (is_printable_unicode(wch)) {
                    // Get position for insertion
                    int atomicPos = getAbsoluteAtomicIndex(cursorY, cursorX, activeSequence);
                    if (atomicPos >= 0) {
                        // Convert single character to null-terminated wide string
                        
                        DEBG_PRINT("Inserting Unicode character: U+%04X '%lc' at position %d\n", 
                                (unsigned int)wch, wch, atomicPos);
                        wchar_t convertedWchar[2];  // Wide string to hold the character + null terminator
                        convertedWchar[0] = (wchar_t)wch;
                        convertedWchar[1] = L'\0';
                        
                        if(cursorX != cursorEndX && cursorY != cursorEndX){
                            
                        }

                        if (insert(activeSequence, atomicPos, convertedWchar) > 0) {
                            cursorX++;
                            cursorEndX == cursorEndX;
                            refreshFlag = true;
                            
                            // Invalidate line stats since text changed
                            setLineStatsNotUpdated();
                        }
                    } else {
                        DEBG_PRINT("Invalid atomic position for insert: %d\n", atomicPos);
                    }
                }// Log unhandled case for debugging
                else {
                    DEBG_PRINT("Unhandled character input: U+%04X (decimal: %d)\n", (unsigned int)wch, (int)wch);
                }
                break;
        }
    }
}

/**
 * Wrapper of 'moveAndUpdateCursor' to easily increment/decrement cursorX/Y variables and update. Resets any range states.
 * Automatically jumps to next/previous line if legal to do so. 
 */
void changeCursor(int incrX, int incrY){
    if(cursorY + incrY < getTotalAmountOfRelativeLines() && cursorY + incrY >= 0){
        cursorY += incrY;
    }
    if(cursorX + incrX <= getUtfNoControlCharCount(cursorY) && cursorX + incrX >= 0){
        cursorX += incrX;
    } else if (cursorX + incrX > getUtfNoControlCharCount(cursorY) && cursorY +1 < getTotalAmountOfRelativeLines()){
        cursorX = 0;
        cursorEndY += 1;
    } else if (cursorX + incrX < 0 && cursorY -1 >= 0){
        cursorY += -1;
        cursorX = getUtfNoControlCharCount(cursorY);
    } else{
        if(incrY != 0 && cursorEndX > getUtfNoControlCharCount(cursorY)){
            cursorX = getUtfNoControlCharCount(cursorY);
        }
        DEBG_PRINT("Invalid case skipped in changeAndUpdateCursor, but range reset.\n");
    }
    cursorEndX = cursorX;
    cursorEndY = cursorY;
}
/**
 * Like changeCursor, but with update. 
 * 
 * Wrapper of 'moveAndUpdateCursor' to easily increment/decrement cursorX/Y variables and update. Resets any range states.
 * Automatically jumps to next/previous line if legal to do so. 
 */
void changeAndUpdateCursor(int incrX, int incrY){
    changeCursor(incrX, incrY);
    updateCursor();
}

/**
 * Updates and moves the cursor state in most efficient manner. Uses the general internal cursorX/Y variables as new position.
 */
void updateCursor(){
    // Just update stats in GUI...
    if (lastGuiHeight >= MENU_HEIGHT) {
        mvprintw(lastGuiHeight - 1, 0, "Ln %d, Col %d   Ctrl-l to quit", cursorY + 1, cursorX + 1);
        clrtoeol(); // Clear rest of status line
    }
    move(cursorY, cursorX);
    refresh();
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