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
int getAbsoluteAtomicIndex(int relativeLine, int targetCharColumn, Sequence* sequence) {
    // Validate input
    if (relativeLine < 0) {
        ERR_PRINT("Invalid relativeLine: %d\n", relativeLine);
        return -1;
    }

    // Ensure absolute positions are computed up to the desired line
    for (int i = 0; i <= relativeLine; i++) {
        if (lineStats.absolutePos[i] == -1) {
            ERR_PRINT("Line %d not available yet.\n", i);
            return -1;
        }
    }

    Position lineStartAbsolutePos = lineStats.absolutePos[relativeLine];

    // Quick return for column 0
    if (targetCharColumn == 0) {
        return lineStartAbsolutePos;
    }

    Atomic *currentItemBlock = NULL;
    Size currentBlockSize = 0;
    Position currentAbsolutePos = lineStartAbsolutePos;
    int charsCounted = 0;
    int bytesConsumedInLine = 0;

    LineBidentifier lineBidentifier = getCurrentLineBidentifier();

    // Count characters until the target column is reached
    while (charsCounted < targetCharColumn) {
        currentBlockSize = getItemBlock(sequence, currentAbsolutePos, &currentItemBlock);
        if (currentBlockSize <= 0 || currentItemBlock == NULL) {
            DEBG_PRINT("Reached end of sequence at pos %lld.\n", (long long)currentAbsolutePos);
            return currentAbsolutePos;
        }

        int blockRelativeOffset = 0;
        while (blockRelativeOffset < currentBlockSize && charsCounted < targetCharColumn) {
            Atomic firstByte = currentItemBlock[blockRelativeOffset];

            // Stop at line break
            if (firstByte == lineBidentifier) {
                DEBG_PRINT("Line break at pos %lld.\n", (long long)currentAbsolutePos);
                return currentAbsolutePos;
            }

            // Determine UTF-8 character length
            int charByteLength = 0;
            if ((firstByte & 0x80) == 0) charByteLength = 1;
            else if ((firstByte & 0xE0) == 0xC0) charByteLength = 2;
            else if ((firstByte & 0xF0) == 0xE0) charByteLength = 3;
            else if ((firstByte & 0xF8) == 0xF0) charByteLength = 4;
            else {
                ERR_PRINT("Invalid UTF-8 byte: 0x%02X at pos %lld.\n", firstByte, (long long)currentAbsolutePos);
                currentAbsolutePos++;
                bytesConsumedInLine++;
                blockRelativeOffset++;
                continue;
            }

            // If character is incomplete, fetch next block
            if (blockRelativeOffset + charByteLength > currentBlockSize) {
                break;
            }

            // Count printable characters (and tabs)
            if (firstByte >= 0x20 || firstByte == '\t') {
                charsCounted++;
            }

            currentAbsolutePos += charByteLength;
            bytesConsumedInLine += charByteLength;
            blockRelativeOffset += charByteLength;

            if (charsCounted == targetCharColumn) {
                DEBG_PRINT("Target column %d at pos %lld.\n", targetCharColumn, (long long)currentAbsolutePos);
                return currentAbsolutePos;
            }
        }
    }

    // Return current position if target wasn't reached (e.g., line too short)
    DEBG_PRINT("End of loop. Returning pos %lld (got %d chars, target %d).\n",
               (long long)currentAbsolutePos, charsCounted, targetCharColumn);
    return currentAbsolutePos;
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