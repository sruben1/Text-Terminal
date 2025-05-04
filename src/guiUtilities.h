#ifndef GUIUTILITIES_H
#define GUIUTILITIES_H
#include <stdbool.h> 
#include <wchar.h>
#include <string.h>

#include "debugUtil.h"
#include "textStructure.h"

static int currentFirstLineAbsoluteIdx;
static bool statsUpToDate = false;

/**
 * Function to translate current screen position to (general) absolute atomic index.
 */
int getAbsoluteAtomicIndex(int relativeLine, int column, Sequence* sequence);

/** 
 * Function that returns a wChar string with L'\0' terminator. 
 * >> sizeToPass == last parsed index of itemArray **+1**.
 * >> precomputedWCharCount == nbr of wChars without the here added null terminator.
 */
wchar_t* utf8_to_wchar(const Atomic* itemArray, int sizeToParse, int precomputedWCharCount);

#endif 