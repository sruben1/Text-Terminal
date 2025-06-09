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

// For horizontal scrolling:
static int _horizontalScreenOffset = 0;

static int _portTopIdxForNext = 0;

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
 * Returns the quantity of lines currently stored in line stats system.
 */
int getTotalAmountOfRelativeLines(){
    int i = 0;
    for(; i <= 75; i++){ // Hard coded internal limit at 75
        if (lineStats.absolutePos[i] == -1){
            return i;
        }
    }
    return i;
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
    DEBG_PRINT("Line stats set to invalidated.\n");
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
    lineStats.absolutePos[relativeLineNumber +1] = -1;
    lineStats.charCount[relativeLineNumber +1] = -1;
    return 1;
}

/**
 * Get Line number at the top of the screen.
 */
int gettopMostLineNbr(){
    return lineStats.topMostLineNbr;
}

/**
 * Function to call when scrolling.
 *  requires full update of statistics afterwards since operation invalidates internal state,
 *  except for first relative screen line due to need of leap of faith. 
 */
ReturnCode moveAbsoluteLineNumbers(Sequence* sequence, int addOrSubstractOne){
    DEBG_PRINT("moveAbsoluteLineNumbers, initial _portTopIdxForNext: %d\n", _portTopIdxForNext);
    if (addOrSubstractOne > 0){
        DEBG_PRINT("Scrolling down\n");
        if(getTotalAmountOfRelativeLines() > 1){
            if (lineStats.absolutePos[1] != -1){
            lineStats.topMostLineNbr++;
            DEBG_PRINT("topMostLineNbr: %d\n", lineStats.topMostLineNbr);
            // Leap of faith:
            _portTopIdxForNext = lineStats.absolutePos[1];
            DEBG_PRINT("Scroll down case _portTopIdxForNext: %d\n", _portTopIdxForNext);
            } else{
                ERR_PRINT("Failed to scroll down, internal state issue...\n");
                // try to recover from bad state...
            }
            
        } else{
            ERR_PRINT("Scroll down illegal!\n");
            return -1;
        }
    } else if (addOrSubstractOne < 0){
        DEBG_PRINT("Scrolling up\n");
        if (lineStats.absolutePos[0] > 0){
            lineStats.topMostLineNbr--;
            _portTopIdxForNext = backtrackToFirstAtomicInLine(sequence, lineStats.absolutePos[0]-1);
            DEBG_PRINT("Scroll up case _portTopIdxForNext: %d\n", _portTopIdxForNext);
        } else{
            ERR_PRINT("Scroll up illegal!\n");
            return -1;
        }
    }else{
        // ignore
    }
    setLineStatsNotUpdated();
    return 1;
}

/**
 * Function to call when jumping to specific line. Counting line numbers form 0. 
 *  requires full update of statistics afterwards since operation invalidates internal state.
 */
ReturnCode jumpAbsoluteLineNumber(int newTopLineNumber, int atomicIdxOfTop){
    lineStats.topMostLineNbr = newTopLineNumber;
    // leap of faith for next print:
    _portTopIdxForNext = atomicIdxOfTop;
    setLineStatsNotUpdated();
    return 1;
}

/**
 * Returns the current horizontal scrolling state, returns integer >= 0. 
 */
int getCurrHorizontalScrollOffset(){
    return _horizontalScreenOffset;
}

/**
 * Used to increment the current horizontal scrolling value, returns 1 on success, -1 on fail. 
 */
ReturnCode changeHorizontalScrollOffset(int increment){
    if(_horizontalScreenOffset + increment < 0){
        _horizontalScreenOffset = 0;
    } else{
        _horizontalScreenOffset += increment;
    }
    return 1;
}

/**
 * Used to set the current horizontal scrolling value if new value < 0 simply set to 0. 
 */
ReturnCode setHorizontalScrollOffset(int newValue){
    if(newValue < 0){
        _horizontalScreenOffset = 0;
    } else{
        _horizontalScreenOffset = newValue;
    }
    return 1;
}

/**
 * Special function to port in  "leap of faith" fashion an atomic index value to next printing round since line stats will be invalidated by then.
 */
int getPrintingPortAtomicPosition(){
    DEBG_PRINT("Requesting top idx for print, is%d\n", _portTopIdxForNext);
    return _portTopIdxForNext;
}

void debugPrintInternalLineStats(){
    DEBG_PRINT(">>>Internal line stats<<<\n");
    DEBG_PRINT("Top most absNbr:%d, Atomic IDX:%d\n", _portTopIdxForNext, lineStats.topMostLineNbr);
    DEBG_PRINT("===================================\n");
    DEBG_PRINT("||Line|scrnCharCount|absAtomicStart\n");
    for(int i = 0; i < 74; i++){
        DEBG_PRINT("|| %02d | %04d        | %04d\n", i, lineStats.charCount[i], lineStats.absolutePos[i]);
        if (lineStats.absolutePos[i] == -1){
            break;
        }
    }
    DEBG_PRINT("===================================\n");
}


/**
 * Function to translate current screen position to (general) absolute atomic index. 
 * >> relativeLine and charColumn require counting form position 0.
 */
int getAbsoluteAtomicIndex(int relativeLine, int charColumn, Sequence* sequence){
    DEBG_PRINT("Calculating abs atomic index for: line%d, column%d...\n", relativeLine, charColumn);
    //debugPrintInternalState(sequence, true, false);
    //debugPrintInternalLineStats();
    // Check that request is valid in current data structure state: 
    for(int i = 0; i <= relativeLine; i++){
        if (lineStats.absolutePos[i] == -1){
            ERR_PRINT("Char position translation is beyond currently stored lines!");
            return -1;
        }
    }
    
    // Quick access case: beginning of line
    if(_horizontalScreenOffset == 0 && charColumn == 0){
        return lineStats.absolutePos[relativeLine];
    } 

    LineBidentifier linBidentifier = getCurrentLineBidentifier();

    int size = 0;
    int blockOffset = 0;
    int charCount = 0;
    int rollingAtomicCount = 0;
    Atomic *currentItemBlock = NULL;

    while (charCount < charColumn + _horizontalScreenOffset +1){
        DEBG_PRINT("rollingAtmcCount:%d, blockOffs:%d, size:%d.\n", rollingAtomicCount, blockOffset, size);
        if(rollingAtomicCount >= size){
            blockOffset = blockOffset + rollingAtomicCount;
            rollingAtomicCount = 0;
            DEBG_PRINT("Requesting next block in seek, at atomic:%d\n", lineStats.absolutePos[relativeLine] + blockOffset);
            size = getItemBlock(sequence, lineStats.absolutePos[relativeLine] + blockOffset, &currentItemBlock);
            DEBG_PRINT("New blockOffset=%d, size=%d\n", blockOffset, size);
            if(size <= 0){
                ERR_PRINT("Position determination failed (on block request for atomic:%d).\n", lineStats.absolutePos[relativeLine] + blockOffset);
                return -1;
            }
        }


        DEBG_PRINT("Attempt readout char...\n");
        DEBG_PRINT("seeking at char%d: '%c'\n", charCount, currentItemBlock[rollingAtomicCount]); 
        if( ((currentItemBlock[rollingAtomicCount] & 0xC0) != 0x80) && (currentItemBlock[rollingAtomicCount] >= 0x20) ){
            // If start of char UTF-8 char and not a control char:
            DEBG_PRINT("Incrementing to char%d\n", charCount +1);
            charCount++;
        }

        //DEBG_PRINT("Cond1:%d, Cond2:%d ;compared to:'%c'\n",(currentItemBlock[rollingAtomicCount] == linBidentifier), (currentItemBlock[rollingAtomicCount] == END_OF_TEXT_CHAR), currentItemBlock[rollingAtomicCount]);
        if(/*rollingAtomicCount < size &&*/ (currentItemBlock[rollingAtomicCount] == linBidentifier || currentItemBlock[rollingAtomicCount] == END_OF_TEXT_CHAR)){
            if (charColumn +_horizontalScreenOffset != charCount){/*Not last column +1 requested case: not comparing to +1 since while loop takin this +1 into account!...*/
                ERR_PRINT("Requested column + horiz scroll:%d is beyond last legal char column:%d\n",charColumn +_horizontalScreenOffset, charCount/*last +1*/ );
                return -1;
            } else{
                // Special case to allow moving cursor after the last char of the line.
                DEBG_PRINT("In special last column +1\n");
                rollingAtomicCount++; // Increment to keep return value consistency among different cases...
                if(getCurrentLineBstd == MSDOS){
                    DEBG_PRINT("Doing special MSDOS line break standard handling...\n");
                    // decrease atomic for return by 1 position to keep \r\n correctly contiguous.
                    rollingAtomicCount--;
                }
            }  
            break;
        } 
        rollingAtomicCount++;
    }
    DEBG_PRINT("Atomic start of line:%d, blockOffs:%d, rollingAtomCont:%d\n",lineStats.absolutePos[relativeLine],blockOffset,rollingAtomicCount);
    return lineStats.absolutePos[relativeLine] + blockOffset + rollingAtomicCount - 1;
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

    //DEBG_PRINT("Pre allocating %d wChar positions.\n", precomputedWCharCount+1);
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
        //BG_PRINT("Got parser size:%d\n", (int) lenOfCurrentParse);
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