#ifndef GUIUTILITIES_H
#define GUIUTILITIES_H

#ifndef CLIPBOARD_H

#define CLIPBOARD_H
#include <stdbool.h> 
#include <wchar.h>
#include <string.h>

#include "textStructure.h"

/*
==================================
   copy paste stuff 
==================================
*/
ReturnCode pasteFromClipboard(Sequence* sequence, int cursorY, int cursorX);
char* getFromXclip(void);

#endif // CLIPBOARD_H

// Copy functions
ReturnCode copyToClipboard(Sequence* sequence, int startY, int startX, int endY, int endX);
ReturnCode copyCurrentLine(Sequence* sequence, int currentY);
wchar_t* extractTextRange(Sequence* sequence, Position startPos, Position endPos);
ReturnCode sendToXclip(const char* text);
/*
==================================
    Line statistics data structure: 
==================================
*/

/**
 * Returns the absolute line nbr from the very start, of a specific screen line.
 * >> The line number requires counting from 0. Returns -1 on error.
 * >> Returns -1 if general state invalid, but does not check if requested relative line (on screen) is beyond range.
 */
int getGeneralLineNbr(int lineNbrOnScreen);

/**
 * Returns the quantity of lines currently stored in line stats system.
 */
int getTotalAmountOfRelativeLines();

/**
 * Returns the number of Utf-8 chars in a given line, the line number requires counting from 0.
 */
int getUtfNoControlCharCount(int relativeLine);

/**
 * Function to translate current screen position to (general) absolute atomic index. 
 * >> relativeLine and charColumn require counting form position 0.
 */
int getAbsoluteAtomicIndex(int relativeLine, int charColumn, Sequence* sequence);

/**
 * Interface to invalidate current line statistics until first line is updated again. 
 */
ReturnCode setLineStatsNotUpdated();

/**
 * Function to update internal line statistics data structure. Line number counting from 0. 
 */
ReturnCode updateLine(int relativeLineNumber, int absoluteGeneralAtomicPosition , int nbrOfUtf8CNoControlChars);

/**
 * Function to call when scrolling.
 *  requires full update of statistics afterwards since operation invalidates internal state.
 */
ReturnCode moveAbsoluteLineNumbers(int addOrSubstractOne);

/**
 * Function to call when scrolling. Counting line numbers form 0. 
 *  requires full update of statistics afterwards since operation invalidates internal state.
 */
ReturnCode jumpAbsoluteLineNumber(int newTopLineNumber, int atomicIdxOfTop);

/**
 * Returns the current horizontal scrolling state, returns integer >= 0. 
 */
int getCurrHorizontalScrollOffset();

/**
 * Used to increment the current horizontal scrolling value, returns 1 on success, -1 on fail. 
 */
ReturnCode changeHorizontalScrollOffset(int increment);
/**
 * Used to set the current horizontal scrolling value, returns 1 on success, -1 on fail. 
 */
ReturnCode setHorizontalScrollOffset(int newValue);

/**
 * Special function to port leap of faith value to next printing round since line stats will be invalidated by then.
 */
int getPrintingPortAtomicPosition();

/*
====================
    W-CHAR utilities:
====================
*/

/*Function that returns a wChar string with L'\0' terminator. 
>> sizeToPass == last parsed index of itemArray **+1**; 
>> precomputedWCharCount == nbr of wChars without the here added null terminator*/
wchar_t* utf8_to_wchar(const Atomic* itemArray, int sizeToParse, int precomputedWCharCount);


#endif 