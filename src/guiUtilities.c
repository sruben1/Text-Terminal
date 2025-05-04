
#include "guiUtilities.h"
/*======== Backend gui data structures ========*/
typedef struct {
    int size;
    int topLineIdx;
    int absolutePos[75];
} LineStats;

// Data structure for currently shown lines' statistics.
LineStats lineStats = { -1, 0, {[0 ... 74] = -1} };

int getAbsoluteAtomicIndex(int relativeLine, int column, Sequence* sequence){


    statsUpToDate = true;
    return 1;
}

/*======== W-CHAR utilities ========*/

/*Function that returns a wChar string with L'\0' terminator. 
>> sizeToPass == last parsed index of itemArray **+1**; 
>> precomputedWCharCount == nbr of wChars without the here added null terminator*/
wchar_t* utf8_to_wchar(const Atomic* itemArray, int sizeToParse, int precomputedWCharCount){
    if(precomputedWCharCount == 0){
        /* Might add extra calculation algorithm here if needed.*/
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
        DEBG_PRINT("Current parser byte: %02x\n", (uint8_t) itemArray[(int)atomicIndx]);
        size_t lenOfCurrentParse = mbrtowc(&wStrToReturn[(int)destIndx], (const char*) &itemArray[(int)atomicIndx], sizeToParse - atomicIndx, &state);
        DEBG_PRINT("Got parser size:%d\n", (int) lenOfCurrentParse);
        if((int) lenOfCurrentParse == -1){
            ERR_PRINT("Encountered invalid utf-8 char while converting!\n");
            wStrToReturn[destIndx] = L'\uFFFD'; //Insert "unknown" character instead.
            destIndx+=1;
            atomicIndx+=1;
        } else if((int) lenOfCurrentParse <= -2) {
            ERR_PRINT("Encountered incomplete or corrupt utf-8 char while converting! stopping parsing now.\n");
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