#include <stdio.h> // Standard I/O
#include <locale.h> // To specify that the utf-8 standard is used 
#include <wchar.h> // UTF-8, wide char hnadling
#include <stdbool.h> //Easy boolean support
#include "textStructure.h"

setlocale(LC_ALL, "en_US.UTF-8"); // Set utf-8 as used standard

/*======== Text sequence datastrucutre ========*/
Sequence* activeSequence = {NULL};
LineBstd currentLineBreakStd = NO_INIT;
LineBidentifier currentLineBidentifier = NONE_ID;

ReturnCode open_and_setup_file(char* file_path){
    //Open(...);
     getCurrentLineBstd();
     getCurrentLineBidentifier();
}

/*======== W-CHAR utilities ========*/

wchar_t* utf8_to_wchar(const Atomic* itemArray, int sizeToParse, int precomputedWCharCount){
    if(precomputedWCharCount == 0){
        /* Might add extra calculation alogrithm here if needed.*/
    }
    for(int i = 1; i< sizeToParse; i++){
        //First 2 cases: ASCII (msb==0); or [msb's == 110; or 1110; or 11110]
    }
    // TODO : preallcoate wchar_t* memory for size given with precomputedWCharCount.
    // fill and return wchar (rememeber correct null termination).

}

/* Prints at most the requested nuber of following lines including the utf-8 char at "firstAtomic". */
ReturnCode print_items_after(Position firstAtomic, int nbrOfLines){
    if ( activeSequence == NULL || currentLineBreakStd == NO_INIT || currentLineBidentifier == NONE_ID ){ return -1; }
    int currLineBcount = 0;
    while( currLineBcount < nbrOfLines ){

        Item* returnedPtr = NULL;
        int size = (int) getItemBlock(activeSequence, firstAtomic, &returnedPtr);
        Atomic* currentItemBlock = (Atomic*)(*returnedPtr);

        if( size < 0 ){ return -1; }//Error!!

        int currentSectionStart = 0; //i.e. offset of nbr of Items form pointer start
        int offsetCounter = 0; //i.e. RUNNING offset of nbr of Items form currentSectionStart
        int nbrOfUtf8Chars = 0;

        while(currLineBcount < nbrOfLines){
            while(currentSectionStart + offsetCounter < size){
                if( (currentItemBlock[currentSectionStart + offsetCounter] & 0xC0) != 0x80 ){
                    nbrOfUtf8Chars++; // adds +1 if current atomic not == 10xxxxxx (see utf-8 specs);
                }
                if(currentItemBlock[currentSectionStart + offsetCounter] == currentLineBidentifier || (currentSectionStart+ offsetCounter) == size-1){
                    if( ((currentLineBreakStd == MSDOS) && ((currentSectionStart + offsetCounter) > 0) && (currentItemBlock[currentSectionStart + offsetCounter-1] != '\r'))){ // Might remove if it causes issues, MSDOS should also work if only check for '\n' characters ('\r' then simply not evaluated).
                        /* Error case, lonely '\n' found despite '\r\n' current standard !*/
                        fprintf(stderr,"Error case, lonely '\n' found despite \"\r\n\" current standard !");
                    }
                    wchar_t* lineToPrint = utf8_to_wchar(&currentItemBlock[currentSectionStart], offsetCounter, nbrOfUtf8Chars);
                    /* 
                    TODO :
                    print out line interpreted as UTF-8 sequence here using : "lineToPrint" and mvaddwstr()?
                    
                    */
                    /* reset/setup for next line iteration: */
                    free(lineToPrint);
                    currLineBcount++;
                    currentSectionStart = offsetCounter;
                    offsetCounter = 0;
                    nbrOfUtf8Chars = 0;
                    break;
                }
                offsetCounter++;
            if(currLineBcount > nbrOfLines){ break; }
            }
        }   
	}
    return 0;
}


/*======== Main implementation ========*/
int main(int argc, char *argv[]){
    open_and_setup_file("TODO");

    return 0;
}