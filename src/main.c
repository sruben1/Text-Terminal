#define _XOPEN_SOURCE_EXTENDED //required for wchar to be supported properly 
#include <stdio.h> // Standard I/O
#include <locale.h> // To specify that the utf-8 standard is used 
#include <wchar.h> // UTF-8, wide char hnadling
#include <string.h> // Utilities used for wide char strings
#include <stdbool.h> //Easy boolean support
#include "textStructure.h"

// curtesy of: https://stackoverflow.com/questions/1941307/debug-print-macro-in-c/67667132#67667132 ; use "-DDEBUG" flag to activate.
#ifdef DEBUG
    #include <signal.h> // Breakpint for debugging
    #define DEBG_PRINT(...) printf(__VA_ARGS__)
    #define SET_BREAK_POINT raise(SIGTRAP)
#else
    #define DEBG_PRINT(...) do {} while (0)
    #define SET_BREAK_POINT
#endif
#define ERR_PRINT(...) fprintf(stderr, "[ERROR:] " __VA_ARGS__)

/*======== Text sequence datastrucutre ========*/
Sequence* activeSequence = {NULL};
LineBstd currentLineBreakStd = NO_INIT;
LineBidentifier currentLineBidentifier = NONE_ID;

ReturnCode open_and_setup_file(char* file_path){
    //Open(...); replaces Empty(...)
    activeSequence = Empty(LINUX);
    currentLineBreakStd = getCurrentLineBstd();
    currentLineBidentifier = getCurrentLineBidentifier();
}

/*======== W-CHAR utilities ========*/

/*Function that returns a wChar string with L'\0' terminator. 
>> sizeToPass == last parsed index of itemArray **+1**; 
>> precomputedWCharCount == nbr of wChars without the here added null terminator*/
wchar_t* utf8_to_wchar(const Atomic* itemArray, int sizeToParse, int precomputedWCharCount){
    if(precomputedWCharCount == 0){
        /* Might add extra calculation alogrithm here if needed.*/
        ERR_PRINT("Compute utf-8 char count not implemented!! Please pass precalculated value with function call.");
        return NULL;
    }   

    DEBG_PRINT("Pre allocating %d wChar positions.", precomputedWCharCount+1);
    wchar_t* wStrToReturn = malloc((precomputedWCharCount + 1) * sizeof(wchar_t));

    if (!wStrToReturn){
        ERR_PRINT("Failed to allocate memory for Wstr!");
        return NULL;
    }

    //Init a state to reflect an empty (NULL bytes) sequence.
    mbstate_t state;
    memset(&state, 0, sizeof(state));

    size_t atomicIndx = 0;
    size_t destIndx = 0;
    
    DEBG_PRINT("Bool: %d\n", (((int)atomicIndx) < sizeToParse) && (((int)destIndx) < precomputedWCharCount));
    while((((int)atomicIndx) < sizeToParse) && (((int)destIndx) < precomputedWCharCount)){
        DEBG_PRINT("Trying to parse\n");
        size_t lenOfCurrentParse = mbrtowc(&wStrToReturn[(int)destIndx], (const char*) &itemArray[(int)atomicIndx], sizeToParse - atomicIndx, &state);
        DEBG_PRINT("Got parser size:%d", (int) lenOfCurrentParse);
        if((int) lenOfCurrentParse == -1){
            ERR_PRINT("Encountered invalid utf-8 char while converting!");
            wStrToReturn[destIndx] = L'\uFFFD'; //Insert "unkonwn" character instead.
            destIndx+=1;
            atomicIndx+=1;
        } else if((int) lenOfCurrentParse <= -2) {
            ERR_PRINT("Encountered incomplete or corrupt utf-8 char while converting! stopping parsing now.");
            break;
        } else {
            //Increment to next utf-8 byte (sequence) start:
            DEBG_PRINT("Incrementing atomic postion by: %d\n", (int) lenOfCurrentParse);
            destIndx+=1;
            atomicIndx += (int)lenOfCurrentParse;
        }
        DEBG_PRINT("Bool: %d\n", (((int)atomicIndx) < sizeToParse) && (((int)destIndx) < precomputedWCharCount));
        //SET_BREAK_POINT;
    }
    //Finalize parse
    if ((destIndx < precomputedWCharCount)){
        ERR_PRINT("Parser did not reach expected nbr of UTF-8 chars: Atomics index %d ; UTF-8 chars index: %d ",(int) atomicIndx,(int) destIndx);
        wStrToReturn[(int)destIndx] = L'\0';
    } else{
        wStrToReturn[(int)precomputedWCharCount] = L'\0';
    }
    return wStrToReturn;
}

/* Prints at most the requested nuber of following lines including the utf-8 char at "firstAtomic". Return code 1: single block accessed; code 2: multiple blocks accessed */
ReturnCode print_items_after(Position firstAtomic, int nbrOfLines){
    DEBG_PRINT("[Trace] : in print function\n");
    if ( activeSequence == NULL || currentLineBreakStd == NO_INIT || currentLineBidentifier == NONE_ID ){ return -1; }
    int currLineBcount = 0;
    bool requestNextBlock = false;
    int size = -1;

    //In order to ensure porting line variables for if split over multiple blocks:
    int atomicsInLine = 0; // not an index! (+1 generaly) 
    int nbrOfUtf8CharsInLine = 0;
    int nbrOfUtf8CharsNoControlCharsInLine = 0; // If we want to ignore line breaks.

    while( currLineBcount < nbrOfLines ){
        DEBG_PRINT("[Trace] : in main while loop, %p %d %d \n", activeSequence, currentLineBreakStd, currentLineBidentifier);
        Atomic* currentItemBlock = NULL;
        if(requestNextBlock){
            DEBG_PRINT("[Trace] : Consecutive block requested\n");
            firstAtomic = firstAtomic + size; // since size == last index +1 no additional +1 needed.
            size = (int) getItemBlock(activeSequence, firstAtomic, &currentItemBlock);
            DEBG_PRINT("The size value %d\n", size);
            if (size < 0){
                return 2;
            }
        } else{
            DEBG_PRINT("[Trace] : First block requested\n");
            size = (int) getItemBlock(activeSequence, firstAtomic, &currentItemBlock);
        }
        

        if(( size < 0 ) || ( currentItemBlock == NULL )){DEBG_PRINT("Main error: size value %d", size); return -1; }//Error!!

        int currentSectionStart = 0; //i.e. offset of nbr of Items form pointer start
        int offsetCounter = 0; //i.e. RUNNING offset of nbr of Items form currentSectionStart
        int nbrOfUtf8Chars = 0;
        int nbrOfUtf8CharsNoControlChars = 0; // If we want to ignore line breaks.

        while((currLineBcount < nbrOfLines) && !requestNextBlock){
            DEBG_PRINT("handling atomic at index: %d, is : '%c' \n", (firstAtomic + currentSectionStart + offsetCounter), currentItemBlock[currentSectionStart + offsetCounter]);

            //TODO : might add seccond variable having ignore cases for \n ,\r, etc. :
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
                DEBG_PRINT("Nuber of UTF-8 chars in this line/end of block = %d \n",  nbrOfUtf8Chars);
                //Decrease, since current char is a line break char:
                if( ((currentLineBreakStd == MSDOS) && ((currentSectionStart + offsetCounter) > 0) && (currentItemBlock[currentSectionStart + offsetCounter-1] != '\r'))){ // Might remove if it causes issues, MSDOS should also work if only check for '\n' characters ('\r' then simply not evaluated).
                    /* Error case, lonely '\n' found despite '\r\n' current standard !*/
                    ERR_PRINT("Warning case, lonely '\n' found despite \"\r\n\" current standard !\n");
                }
                wchar_t* lineToPrint = utf8_to_wchar(&currentItemBlock[currentSectionStart], offsetCounter+1, nbrOfUtf8Chars);

                /* 
                TODO :
                print out line or block (could be either!!), interpreted as UTF-8 sequence here using : "lineToPrint" and mvaddwstr()?
                
                */


                #ifdef DEBUG
                // Basic test print to test backend:
                DEBG_PRINT("section start = %d, Offset = %d \n", currentSectionStart, offsetCounter);
                char* textContent = (char*)currentItemBlock;
                DEBG_PRINT("~~~~~~~~~~~~~~~~~~\n");
                for (size_t i = 0; lineToPrint[i] != L'\0'; i++) {
                    if(lineToPrint[i] != L'\n'){
                        putwchar(lineToPrint[i]);
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


                /* reset/setup for next block/line iteration: */
                free(lineToPrint);
                if ((currentItemBlock[currentSectionStart + offsetCounter] == currentLineBidentifier) || (currentItemBlock[currentSectionStart + offsetCounter] == END_OF_TEXT_CHAR)){
                    // handle this blocks line stats:
                    atomicsInLine += offsetCounter+1;
                    nbrOfUtf8CharsInLine += nbrOfUtf8Chars;
                    nbrOfUtf8CharsNoControlCharsInLine += nbrOfUtf8CharsNoControlChars;
                    /*
                      >>> TODO: take in line stats (from variables) here
                    */
                   
                    ++currLineBcount; // Includues the current line; please ensure to keep the incrementation by 1 here...
                    nbrOfUtf8CharsNoControlCharsInLine; // Use this if should not count any '\n' '\r' ... chars
                    nbrOfUtf8CharsInLine;

                    DEBG_PRINT("atomicsInLine: %d, nbrOfUtf8CharsInLine: %d, nbrOfUtf8CharsNoControlCharsInLine: %d\n", atomicsInLine, nbrOfUtf8CharsInLine, nbrOfUtf8CharsNoControlCharsInLine);
                    // Reset variables for next line:
                    atomicsInLine = 0;
                    nbrOfUtf8CharsInLine = 0;
                    nbrOfUtf8CharsNoControlCharsInLine = 0;
                } else{
                    // Ensure port of line statistics to next block handling (iteration):
                    atomicsInLine += offsetCounter+1;
                    nbrOfUtf8CharsInLine += nbrOfUtf8Chars;
                    nbrOfUtf8CharsNoControlCharsInLine += nbrOfUtf8CharsNoControlChars;
                    DEBG_PRINT("Ported, current state: atomicsInLine: %d, nbrOfUtf8CharsInLine: %d, nbrOfUtf8CharsNoControlCharsInLine: %d\n", atomicsInLine, nbrOfUtf8CharsInLine, nbrOfUtf8CharsNoControlCharsInLine);
                }
                currentSectionStart = currentSectionStart + offsetCounter + 1;
                offsetCounter = -1;
                nbrOfUtf8Chars = 0;
                nbrOfUtf8CharsNoControlChars = 0;
            }
            offsetCounter++;
            if((currentSectionStart + offsetCounter >= size)){
                DEBG_PRINT("Setting flag for consecutive request");
                requestNextBlock = true;
            }
        }   
	}
    return 1;
}


/*======== Main implementation ========*/
int main(int argc, char *argv[]){
    DEBG_PRINT("initialized!\n");
    setlocale(LC_ALL, "en_US.UTF-8"); // Set utf-8 as used standard
    open_and_setup_file("TODO");
    print_items_after(0, 2);
    return 0;
}