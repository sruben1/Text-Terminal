#ifndef GUIUTILITIES_H
#define GUIUTILITIES_H
#include <stdbool.h> 
#include <wchar.h>
#include <string.h>

#include "textStructure.h"

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
ReturnCode moveAbsoluteLineNumbers(int addOrSubstract);

/**
 * Function to call when scrolling. Counting line numbers form 0. 
 *  requires full update of statistics afterwards since operation invalidates internal state.
 */
ReturnCode setAbsoluteLineNumber(int newLineNumber);

/*
====================
    W-CHAR utilities:
====================
*/

/** 
 * Function that returns a wChar string with L'\0' terminator. 
 * >> sizeToParse == last parsed index of itemArray **+1**.
 * >> precomputedWCharCount == nbr of wChars without the here added null terminator.
 */
wchar_t* utf8_to_wchar(const Atomic* itemArray, int sizeToParse, int precomputedWCharCount);

#endif 