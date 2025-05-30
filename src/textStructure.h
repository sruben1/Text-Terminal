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

typedef int ReturnCode; /* 1: success; negative: failure; 0: undefined*/
typedef int Position; /* a position in the sequence */
typedef int Size; /* a length measurement (size == last index +1, if first index == 0) */
typedef uint8_t Atomic;/* 1 bytes (warning: is smaller then the atomic size of some utf-8 character (since up to 4 bytes for 1 utf-8 char)) */
typedef void *Item; 
typedef enum { LINUX, MSDOS, MAC, NO_INIT} LineBstd;
typedef enum { LINUX_MSDOS_ID='\n', MAC_ID='\r', NONE_ID='\0'} LineBidentifier;

/*
=========================
  Main Text-Sequence
=========================
*/

/* Linked list node */
typedef struct DescriptorNode DescriptorNode;
struct DescriptorNode{
  DescriptorNode *next_ptr;
  DescriptorNode *prev_ptr;
  bool isInFileBuffer;
  unsigned long offset;
  unsigned long size;
};

/* Piece table as a linked list */
typedef struct{
  DescriptorNode *first;
  DescriptorNode *last; 
  int length;
} PieceTable;

/* Buffer for storing text */
typedef struct{
  Atomic *data;
  size_t size; // occupied space
  size_t capacity; // allocated space
} Buffer;

/* Combined data structure */
typedef struct {
  PieceTable pieceTable;
  Buffer fileBuffer;
  Buffer addBuffer;
} Sequence;

/*
=========================
  Setup
=========================
*/
Sequence* empty(); /* Create an empty sequence (i.e. for new empty file)*/
Sequence* loadOrCreateNewFile( char *pathname ); /* i.e. open a file */
LineBstd getCurrentLineBstd(); 
LineBidentifier getCurrentLineBidentifier(); /* returns '\n' for Linux & MSDOS or '\r' for MAC */

/*
=========================
  Quit
=========================
*/
ReturnCode saveSequence( char *pathname, Sequence *sequence );
ReturnCode closeSequence( Sequence *sequence, bool forceFlag ); /* Free all resources of specified sequence, forceFlag: 1 -> Close even if not saved, 0 -> Close only when saved */

/*
=========================
  Read
=========================
*/
Size getItemBlock( Sequence *sequence, Position position, Atomic **returnedItemBlock); /* More efficient when retrieving multiple consecutive Items , if multiple Items are already stored in a consecutive block. Size == last index + 1 (of return Block) or -1 to indicate error.*/


/*
=========================
  Query internals
=========================
*/
int getCurrentWordCount();

/*
=========================
  Write/Edit
=========================
*/
ReturnCode insert( Sequence *sequence, Position position, wchar_t *textToInsert ); /* textToInsert: nullterminated string of wide chars */
ReturnCode delete( Sequence *sequence, Position beginPosition, Position endPosition );

ReturnCode copy( Sequence *sequence, Position fromBegin, Position fromEnd, Position toPosition );
ReturnCode moveSequence( Sequence *sequence, Position fromBegin, Position fromEnd, Position toPosition );

ReturnCode undo( Sequence *sequence);
ReturnCode redo( Sequence *sequence);


/*
=============
  Debug Utils
=============
*/
void debugPrintInternalState(Sequence* sequence, bool showAddBuff, bool showFileBuff);
#endif