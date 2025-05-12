#include "guiUtilities.h"
#include "textStructure.h"
#include "debugUtil.h"

/*
==================================
    Line statistics data structure: 
==================================
*/

// Data structure for currently shown lines' statistics.
typedef struct {
    // Currently shown upper most line's line number counted from the very beginning of text:
    int topMostLineNbr;
    // Number of UTF-8 chars in line without counting control chars:
    int charCount[75]; 
    // Absolute atomic position of curent lines:
    int absolutePos[75]; // here -1 consistently inserted into last index +1 => if at index 0 value == -1 -> signifies not in update state! 
} LineStats;
LineStats lineStats = {
    .topMostLineNbr = 0,
    .charCount = {[0 ... 74] = 0},
    .absolutePos = {[0 ... 74] = -1}
};

/**
 * Returns the absolute line nbr from the very start, of a specific screen line.
 * >> The line number requires counting from 0. Returns -1 on error.
 * >> Returns -1 if general state invalid, but does not check if requested relative line (on screen) is beyond range.
 */
int getGeneralLineNbr(int lineNbrOnScreen){
    // Make sure internal state is indeed updated:
    if (lineStats.absolutePos[0] != -1){ 
        return lineStats.topMostLineNbr + lineNbrOnScreen;
    } else{
        return -1;
    }
}

/**
 * Returns the number of Utf-8 chars in a given line, the line number requires counting from 0.
 */
int getUtfNoControlCharCount(int relativeLine){
    // Make sure internal state is indeed updated:
    if (lineStats.absolutePos[0] != -1){ 
        return lineStats.charCount[relativeLine];
    } else{
        return -1;
    }
}

/**
 * Interface to invalidate current line statistics until first line is updated again. 
 */
ReturnCode setLineStatsNotUpdated(){
    lineStats.absolutePos[0] = -1;
    return 1;
}

/**
 * Function to update internal line statistics data structure. Relative line number counting from 0. 
 */
ReturnCode updateLine(int relativeLineNumber, int absoluteGeneralAtomicPosition , int nbrOfUtf8CNoControlChars){
    DEBG_PRINT("[Line Stats] : Updated line nbr %d to Atomic Idx: %d, charCount: %d.\n", relativeLineNumber, absoluteGeneralAtomicPosition, nbrOfUtf8CNoControlChars);
    lineStats.absolutePos[relativeLineNumber] = absoluteGeneralAtomicPosition;
    lineStats.charCount[relativeLineNumber] = nbrOfUtf8CNoControlChars;
    return 1;
}

/**
 * Function to call when scrolling.
 *  requires full update of statistics afterwards since operation invalidates internal state.
 */
ReturnCode moveAbsoluteLineNumbers(int addOrSubstract){
    setLineStatsNotUpdated();
    lineStats.topMostLineNbr += addOrSubstract;
    return 1;
}

/**
 * Function to call when jumping to specific line. Counting line numbers form 0. 
 *  requires full update of statistics afterwards since operation invalidates internal state.
 */
ReturnCode setAbsoluteLineNumber(int newLineNumber){
    setLineStatsNotUpdated();
    lineStats.topMostLineNbr = newLineNumber;
    return 1;
}

/**
 * Function to translate current screen position to (general) absolute atomic index. 
 * >> relativeLine and charColumn require counting form position 0.
 */
int getAbsoluteAtomicIndex(int relativeLine, int charColumn, Sequence* sequence){
    
    // Check that request is valid in current data structure state: 
    for(int i = 0; i <= relativeLine; i++){
        if (lineStats.absolutePos[i] == -1){
            ERR_PRINT("Char position translation is beyond currently stored lines!");
            return -1;
        }
    }
    
    // quick access case:
    if(charColumn == 0){
        return lineStats.absolutePos[relativeLine];
    } 

    // General case, determine by iterating through chars:
    Atomic *currentItemBlock = NULL;

    int atomicIndex = 0;

    // Get first block of the line
    Size size = getItemBlock(sequence, lineStats.absolutePos[relativeLine], &currentItemBlock);
    if(size < 0){
        ERR_PRINT("Position determination failed (on first block request).\n");
        return -1;
    }

    LineBidentifier lineBidentifier = getCurrentLineBidentifier();

    int charCounter = 0;
    int blockOffset = 0;
    while (atomicIndex < charColumn){
        if(atomicIndex >= size + blockOffset){
            // get next block
            size = getItemBlock(sequence, lineStats.absolutePos[relativeLine] + atomicIndex, &currentItemBlock);
            if(size < 0){
                ERR_PRINT("Position determination failed (on consecutive block request).\n");
                return -1;
            }
        }
        DEBG_PRINT("seeking at char: '%c'\n", currentItemBlock[atomicIndex]); 
        if(currentItemBlock[atomicIndex] == lineBidentifier){
            // found line end (char)
            ERR_PRINT("Line shorter then requested column postion!\n");
            return -1;
        }
        if( ((currentItemBlock[atomicIndex] & 0xC0) != 0x80) && (currentItemBlock[atomicIndex] >= 0x20) ){
            // If start of char UTF-8 char and not a control char:
            charCounter++;
        }
        atomicIndex++;
    }
    DEBG_PRINT("Seek ended with blockSize = %d, blockOffset = %d, nbr of chars %d\n", size, blockOffset, charCounter);
    return charCounter;
}


/*
====================
    W-CHAR utilities:
====================
*/

/*Function that returns a wChar string with L'\0' terminator. 
>> sizeToPass == last parsed index of itemArray **+1**; 
>> precomputedWCharCount == nbr of wChars without the here added null terminator*/
wchar_t* utf8_to_wchar(const Atomic* itemArray, int sizeToParse, int precomputedWCharCount){
    if(precomputedWCharCount == 0){
        /* Might add extra calculation algorithm here if needed.*/
        ERR_PRINT("Compute utf-8 char count not implemented!! Please pass precalculated value with function call.\n");
        return NULL;
    }   

    DEBG_PRINT("Pre allocating %d wChar positions.\n", precomputedWCharCount+1);
    wchar_t* wStrToReturn = malloc((precomputedWCharCount + 1) * sizeof(wchar_t));

    if (!wStrToReturn){
        ERR_PRINT("Failed to allocate memory for Wstr!\n");
        return NULL;
    }

    //Init a state to reflect an empty (NULL bytes) sequence.
    mbstate_t state;
    memset(&state, 0, sizeof(state));

    size_t atomicIndx = 0;
    size_t destIndx = 0;
    
    //DEBG_PRINT("Bool: %d\n", (((int)atomicIndx) < sizeToParse) && (((int)destIndx) < precomputedWCharCount));
    while((((int)atomicIndx) < sizeToParse) && (((int)destIndx) < precomputedWCharCount)){
        //DEBG_PRINT("Trying to parse\n");
        //DEBG_PRINT("Current parser byte: %02x\n", (uint8_t) itemArray[(int)atomicIndx]);
        size_t lenOfCurrentParse = mbrtowc(&wStrToReturn[(int)destIndx], (const char*) &itemArray[(int)atomicIndx], sizeToParse - atomicIndx, &state);
        //DEBG_PRINT("Got parser size:%d\n", (int) lenOfCurrentParse);
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
            //DEBG_PRINT("Incrementing atomic postion by: %d\n", (int) lenOfCurrentParse);
            destIndx+=1;
            atomicIndx += (int)lenOfCurrentParse;
        }
        //DEBG_PRINT("Bool: %d\n", (((int)atomicIndx) < sizeToParse) && (((int)destIndx) < precomputedWCharCount));
        //SET_BREAK_POINT;
    }
    //Finalize parse
    if (( (int) destIndx < precomputedWCharCount)){
        ERR_PRINT("Parser did not reach expected nbr of UTF-8 chars: Atomics index %d ; UTF-8 chars index: %d\n",(int) atomicIndx,(int) destIndx);
        wStrToReturn[(int)destIndx] = L'\0';
    } else{
        wStrToReturn[(int)precomputedWCharCount] = L'\0';
    }
    return wStrToReturn;
}