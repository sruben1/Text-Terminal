#define _XOPEN_SOURCE_EXTENDED //required for wchar to be supported properly 
#include <stdio.h> // Standard I/O
#include <locale.h> // To specify that the utf-8 standard is used 
#include <wchar.h> // UTF-8, wide char hnadling
#include <stdbool.h> //Easy boolean support
#include "textStructure.h"

// curtesy of: https://stackoverflow.com/questions/1941307/debug-print-macro-in-c/67667132#67667132 ; use "-DDEBUG" flag to activate.
#ifdef DEBUG
    #define DEBG_PRINT(...) printf(__VA_ARGS__)
#else
    #define DEBG_PRINT(...) do {} while (0)
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

wchar_t* utf8_to_wchar(const Atomic* itemArray, int sizeToParse, int precomputedWCharCount){

    //TEMPORARY:
    return (wchar_t*) malloc(32);



    if(precomputedWCharCount == 0){
        /* Might add extra calculation alogrithm here if needed.*/
    }
    for(int i = 1; i< sizeToParse; i++){
        //First 2 cases: ASCII (msb==0); or [msb's == 110; or 1110; or 11110]
    }
    // TODO : preallcoate wchar_t* memory for size given with precomputedWCharCount.
    // fill and return wchar (rememeber correct null termination).
}

/* Prints at most the requested nuber of following lines including the utf-8 char at "firstAtomic". Return code 1: single block accessed; code 2: multiple blocks accessed */
ReturnCode print_items_after(Position firstAtomic, int nbrOfLines){
    DEBG_PRINT("[Trace] : in print function\n");
    if ( activeSequence == NULL || currentLineBreakStd == NO_INIT || currentLineBidentifier == NONE_ID ){ return -1; }
    int currLineBcount = 0;
    bool requestNextBlock = false;
    int size = -1;

    while( currLineBcount < nbrOfLines ){
        DEBG_PRINT("[Trace] : in main while loop, %p %d %d \n", activeSequence, currentLineBreakStd, currentLineBidentifier);
        Atomic* currentItemBlock = NULL;
        if(requestNextBlock){
            DEBG_PRINT("[Trace] : Consecutive block requested");
            firstAtomic = firstAtomic + size; // since size == last index +1 no additional +1 needed.
            size = (int) getItemBlock(activeSequence, firstAtomic, &currentItemBlock);
            DEBG_PRINT("The size value %d", size);
            if (size < 0){
                return 2;
            }
        } else{
            DEBG_PRINT("[Trace] : First block requested");
            size = (int) getItemBlock(activeSequence, firstAtomic, &currentItemBlock);
        }
        

        if(( size < 0 ) || ( currentItemBlock == NULL )){DEBG_PRINT("Main error: size value %d", size); return -1; }//Error!!

        int currentSectionStart = 0; //i.e. offset of nbr of Items form pointer start
        int offsetCounter = 0; //i.e. RUNNING offset of nbr of Items form currentSectionStart
        int nbrOfUtf8Chars = 0;

        while((currLineBcount < nbrOfLines) && !requestNextBlock){
            DEBG_PRINT("handling atomic at index: %d, is : '%c' \n", (firstAtomic + currentSectionStart + offsetCounter), currentItemBlock[currentSectionStart + offsetCounter]);

            //TODO : might add seccond variable having ignore cases for \n ,\r, etc. :
            if( (currentItemBlock[currentSectionStart + offsetCounter] & 0xC0) != 0x80 ){
                nbrOfUtf8Chars++; // adds +1, if current atomic not == 10xxxxxx (see utf-8 specs);
            }
            if((currentItemBlock[currentSectionStart + offsetCounter] == currentLineBidentifier) || ((currentSectionStart+ offsetCounter) == size-1)){
                DEBG_PRINT("found a line (or at the end)! current line count = %d \n", (currLineBcount +1));
                DEBG_PRINT("Nuber of UTF-8 chars in this line = %d \n",  nbrOfUtf8Chars);
                if( ((currentLineBreakStd == MSDOS) && ((currentSectionStart + offsetCounter) > 0) && (currentItemBlock[currentSectionStart + offsetCounter-1] != '\r'))){ // Might remove if it causes issues, MSDOS should also work if only check for '\n' characters ('\r' then simply not evaluated).
                    /* Error case, lonely '\n' found despite '\r\n' current standard !*/
                    ERR_PRINT("Error case, lonely '\n' found despite \"\r\n\" current standard !\n");
                }
                wchar_t* lineToPrint = utf8_to_wchar(&currentItemBlock[currentSectionStart], offsetCounter, nbrOfUtf8Chars);
                /* 
                TODO :
                print out line interpreted as UTF-8 sequence here using : "lineToPrint" and mvaddwstr()?
                
                */


                #ifdef DEBUG
                // TEMPORARY test print:
                DEBG_PRINT("section start = %d, Offset = %d \n", currentSectionStart, offsetCounter);
                char* textContent = (char*)currentItemBlock;
                DEBG_PRINT("~~~~~~~~~~~~~~~~~~\n");
                for (int i = currentSectionStart; i <= currentSectionStart + offsetCounter; i++) {
                    if(textContent[i] == currentLineBidentifier){
                        printf("[\\n]");
                    } else{
                        putchar(textContent[i]);
                    }
                }
                DEBG_PRINT("\n~~~~~~~~~~~~~~~~~~\n");
                for (int i = currentSectionStart; i <= currentSectionStart + offsetCounter; i++) {
                    printf("| %02x |", (uint8_t) textContent[i]);
                }
                DEBG_PRINT("\n~~~~~~~~~~~~~~~~~~\n");
                #endif


                /* reset/setup for next line iteration: */
                free(lineToPrint);
                currLineBcount++;
                currentSectionStart = currentSectionStart + offsetCounter + 1;
                offsetCounter = -1;
                nbrOfUtf8Chars = 0;
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
    print_items_after(0, 8);
    return 0;
}