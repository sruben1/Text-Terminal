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
bool is_printable_unicode(wint_t wch);

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

// Helper functions:
ReturnCode deleteCurrentSelectionRange();


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
        return -1;
    }
    
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
    
    // Validate screen size
    if (lastGuiHeight < 3 || lastGuiWidth < 10) {
        endwin();
        fprintf(stderr, "Error: terminal too small (need at least 3x10)\n");
        exit(EXIT_FAILURE);
    }
    
    // Initialize line statistics for first (initial) iteration
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
        int pos = -1; // Used for delete and backspace
        switch (wch){
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
                changeAndupdateCursorAndMenu(1,0);              
                break;
            case KEY_BACKSPACE: // Backspace
            case 8:
                DEBG_PRINT("Processing 'BACKSPACE'\n");

                // Calculation with range support:
                if (cursorNotInRangeSelectionState()){
                    if((cursorY > 0) && (cursorX == 0)){
                        // Case of at begining of a line:
                        DEBG_PRINT("Backspace remove '\n' case...\n");
                        pos = getAbsoluteAtomicIndex(cursorY,0, activeSequence)-1;
                        DEBG_PRINT("Pos would have been: %d but now %d",pos +1, pos);
                        //debugPrintInternalState(activeSequence, true, false);

                        // so that automatically placed at previous line end:
                        relocateCursorNoUpdate(-1, cursorY);
                    } else if (!((cursorX == 0) && (cursorY == 0))){
                        //all other valid cases:
                        DEBG_PRINT("Backspace standard case...\n");
                        pos = getAbsoluteAtomicIndex(cursorY, cursorX -1, activeSequence);

                        // Reposition cursor:
                        relocateCursorNoUpdate(cursorX-1, cursorY);
                    }  else{
                        DEBG_PRINT("BACKSPACE invalid case...\n");
                        break;
                    }

                    DEBG_PRINT("Backspace with atomics: %d to %d\n", pos, pos);
                    if(delete(activeSequence, pos, pos) < 0) {
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
                        // Case of at end of a line:
                        pos = getAbsoluteAtomicIndex(cursorY + 1, 0, activeSequence)-1;
                        DEBG_PRINT("Pos would have been: %d but now %d",pos +1, pos);
                        //debugPrintInternalState(activeSequence, true, false);

                        // No cursor change/relocate needed due to behavior of DELETE.
                    } else if (!((cursorY == getTotalAmountOfRelativeLines() -1) && (cursorX == getUtfNoControlCharCount(cursorY)))){
                            //all other valid cases:
                            DEBG_PRINT("Delete standard case...\n");
                            pos = getAbsoluteAtomicIndex(cursorY, cursorX, activeSequence);
                    } else{
                        DEBG_PRINT("DELETE invalid case...\n");
                        break;
                    }
                    DEBG_PRINT("Delete with atomics: %d to %d\n", pos, pos);
                    if(delete(activeSequence, pos, pos) < 0) {
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
                    // Ensure section state is correctly handled first:
                    if(deleteCurrentSelectionRange() < 0){
                        ERR_PRINT("Aborted INSERT to range delete fail.\n");
                        break;
                    }

                    // Get position for insertion
                    int atomicPos = getAbsoluteAtomicIndex(cursorY, cursorX, activeSequence);
                    if (atomicPos >= 0) {
                        // Convert single character to null-terminated wide string
                        
                        DEBG_PRINT("Inserting Unicode character: U+%04X '%lc' at position %d\n", 
                                (unsigned int)wch, wch, atomicPos);
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

/* ----- Increment cursor -----*/
/**
 * Function to easily and SAFELY increment/decrement by relative values (only +1 or -1 have good support) 
 * (e.g. incrX has the internal effect: x = x + incrX) cursorX/Y variables and update. Resets any range states.
 * Automatically jumps to next/previous line if legal to do so. 
 */
void changeAndupdateCursorAndMenu(int incrX, int incrY){
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
        cursorX = 0;
        cursorY += 1;
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
    // Handle Y:
    int amtOfRelativeLines = getTotalAmountOfRelativeLines();
    if(newY < amtOfRelativeLines  && newY >= 0){
        // Standard case (x handled in next big if block):
        cursorY = newY;
    } else if (newY < 0){
        // Can't take negatives, just put at beginning:
        cursorY = 0;
        cursorX = 0;
        resetRangeSelectionState();
        return;
    } else if (newY >= amtOfRelativeLines) {
        // Special case of beyond last line:
        cursorY = amtOfRelativeLines -1;
        cursorX = getUtfNoControlCharCount(cursorY);
        resetRangeSelectionState();
        return;
    } else {
        ERR_PRINT("Unexpected, unhandled case in 'relocateRangeEndAndUpdate()'\n");
        return;
    }

    // Handle X (with respect to new Y): 
    int charCountAtY = getUtfNoControlCharCount(cursorY);
    if(newX <= charCountAtY && newX >= 0){
        cursorX = newX;
    } else if (newX > charCountAtY && cursorY +1 < amtOfRelativeLines){
        // Beyond line end:
        cursorX = 0;
        cursorEndY += 1;
    } else if (newX < 0 && cursorY -1 >= 0){
        // Go to previous since before first char of line.
        cursorY += -1;
        cursorX = getUtfNoControlCharCount(cursorY);
    } else{
        if(newX > charCountAtY){
            cursorX = charCountAtY;
        } else {
            DEBG_PRINT("Invalid case skipped in 'relocateRangeEndAndUpdate()', but update performed.\n");
        }
    }
    updateCursorAndMenu();
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
        // Perform this to ensure correct ranges still ensured:
        relocateCursorNoUpdate(cursorX,cursorY);

        if (lastGuiHeight >= MENU_HEIGHT) {
            mvprintw(lastGuiHeight - 1, 0, "Ln %d, Col %d   Ctrl-l to quit", cursorY + 1, cursorX + 1);
            clrtoeol(); // Clear rest of status line
        }
    } else{

        int startX = -1, startY = -1, endX = -1, endY = -1;
        getCurrentSelectionRang(&startX, &endX, &startY, &endY);
        if (startY == endY){
            mvchgat(endY, startX, endX-startX, A_REVERSE, 0, NULL); // Format in inverted color scheme
        } else{
            mvchgat(startY, startX, -1, A_REVERSE, 0, NULL);
            mvchgat(endY, 0, endX, A_REVERSE, 0, NULL);
            for (int y = startY+1; y <= endY -1; y++){
                mvchgat(y, 0, -1,A_REVERSE, 0, NULL);
            }
        }
        
        if (lastGuiHeight >= MENU_HEIGHT) {
            mvprintw(lastGuiHeight - 1, 0, "Ln %d-%d, Col %d-%d   Ctrl-l to quit", cursorY + 1, cursorEndY +1, cursorX + 1, cursorEndX +1);
            clrtoeol(); // Clear rest of status line
        }

    }
    move(cursorY, cursorX);
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
        if (delete(activeSequence, getAbsoluteAtomicIndex(startY,startX,activeSequence), getAbsoluteAtomicIndex(endY,endX,activeSequence)) < 0 ){
            ERR_PRINT("Failed to delete what's in selection range!\n");
            return -1;
        }
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