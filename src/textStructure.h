// textStructure.h
#ifndef TEXTSTRUCTURE_H
#define TEXTSTRUCTURE_H
#include <stdint.h>  // For fixed size integer types
#include <stdlib.h> // malloc() etc.
#include <stdbool.h> //Easy boolean support
#include <wchar.h> // wide character support for utf-8
/*
* Main piece table data structure of the text editor. 
* Some adjacent functionality is also included in this file.
* Some of the fundamental principles were developed with regards to: https://www.cs.unm.edu/~crowley/papers/sds/sds.html
*/

#define END_OF_TEXT_CHAR 0x03

typedef int ReturnCode; /* 1: success; negative: failure; 0: undefined */
typedef int Position; /* a position in the sequence */
typedef int Size; /* a length measurement (size == last index +1, if first index == 0) */
typedef uint8_t Atomic; /* 1 byte (warning: is smaller then the atomic size of some utf-8 character (since up to 4 bytes for 1 utf-8 char)) */
typedef void *Item; 
typedef enum {
  LINUX, 
  MSDOS, 
  MAC, 
  NO_INIT
} LineBstd;
typedef enum {
  LINUX_MSDOS_ID='\n',
  MAC_ID='\r',
  NONE_ID='\0'
} LineBidentifier;

/*
=========================
  Main Text-Sequence
=========================
*/

/* Linked list node */
typedef struct DescriptorNode DescriptorNode;
struct DescriptorNode {
  DescriptorNode *next_ptr;
  DescriptorNode *prev_ptr;
  bool isInFileBuffer;
  unsigned long offset;
  unsigned long size;
};

/* Piece table as a linked list */
typedef struct {
  DescriptorNode *first;
  DescriptorNode *last; 
} PieceTable;

/* Buffer for storing text */
typedef struct {
  Atomic *data;
  size_t size; // occupied space
  size_t capacity; // allocated space
} Buffer;

/* Stack for keeping track of operations for undo/redo */
typedef struct OperationStack OperationStack;

/* Position and line number of a text in the sequence */
typedef struct {
  Position foundPosition; // Position of the first character of the found text
  int lineNumber; // Line number of the found text
} SearchResult;

/* Cache entry representing the last insertion */
typedef struct {
  int lastAtomicPos;
  int lastCharSize;
  long int lastWritePos;
} LastInsert;

/* Combined data structure */
typedef struct {
  PieceTable pieceTable;
  Buffer fileBuffer;
  Buffer addBuffer;
  OperationStack *undoStack;
  OperationStack *redoStack;
  int wordCount;
  int lineCount;
  SearchResult lastLineResult; // Internal cache
  LastInsert lastInsert; // Internal cache
} Sequence;

/*
=========================
  Setup
=========================
*/

/**
 * Creates an empty sequence (i.e. for a new empty file).
 */
Sequence* empty();

/**
 * Creates a new sequence from an existing file.
 */
Sequence* loadOrCreateNewFile(char *pathname);

LineBstd getCurrentLineBstd(); 

/**
 * Returns '\n' for Linux & MSDOS or '\r' for MAC.
 */
LineBidentifier getCurrentLineBidentifier();

/*
=========================
  Quit
=========================
*/

ReturnCode saveSequence(char *pathname, Sequence *sequence);

/**
 * Free all resources of the specified sequence.
 * If forceFlag is set to 1, the sequence will be closed even if it has not been saved.
 * If forceFlag is set to 0, the sequence will only be closed if it has been saved.
 */
ReturnCode closeSequence(Sequence *sequence, bool forceFlag);

/*
=========================
  Read
=========================
*/

/**
 * Provides an efficient way to retrieve multiple atomics at once.
 * The pointer given by returnedItemBlock will point to the text block at the specified (atomic) position.
 * If the position is directly at the end of the sequence, it will point to the special END_OF_TEXT_CHAR.
 * Returns the amount of atomics in the block or -1 if an error occurred.
 */
Size getItemBlock(Sequence *sequence, Position position, Atomic **returnedItemBlock);

/*
=========================
  Query internals
=========================
*/

int getCurrentWordCount(Sequence *sequence);
int getCurrentLineCount(Sequence *sequence);

/**
 * Returns the position of the first atomic in the line that contains the specified position.
 * The previous line break is not considered part of the line, i.e. the returned position comes right after the line break.
 * If it is the first line, the position will be 0.
 */
Position backtrackToFirstAtomicInLine(Sequence *sequence, Position position);

/*
=========================
  Write/Edit
=========================
*/

/**
 * Inserts a given text (nullterminated string of wide chars) at the specified position in the sequence.
 */
ReturnCode insert(Sequence *sequence, Position position, wchar_t *textToInsert);

/**
 * Deletes a range of text from the sequence, specified by beginPosition and endPosition.
 * Both positions are inclusive.
 */
ReturnCode delete(Sequence *sequence, Position beginPosition, Position endPosition);

/**
 * Searches for a given text (nullterminated string of wide chars) in the sequence.
 * The search starts at startPosition (inclusive) and, if necessary, wraps around to the beginning of the sequence.
 * Returns the SearchResult for the first charcter of the first occurrence, or -1 if no match was found.
 */
SearchResult find(Sequence *sequence, wchar_t *textToFind, Position startPosition);

/**
 * Searches for a given text (nullterminated string of wide chars) in the sequence.
 * The first occurence after startPosition (inclusive) is deleted and replaced with the specified replacement text.
 * Returns the SearchResult for the first character that has been replaced, or -1 if no match was found.
 */
SearchResult findAndReplace(Sequence *sequence, wchar_t *textToFind, wchar_t *textToReplace, Position startPosition); /* textToFind, textToReplace: nullterminated strings of wide chars, startPosition: position to start searching from */

/*
=========================
  Debug Utils
=========================
*/

/**
 * Prints the internal state of the sequence, including word count, line count, and the state of the piece table.
 * If showAddBuff is true, the content of the add buffer is printed.
 * If showFileBuff is true, the content of the file buffer is printed.
 */
void debugPrintInternalState(Sequence* sequence, bool showAddBuff, bool showFileBuff);

#endif