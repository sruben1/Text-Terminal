// textStructure.h
#ifndef TEXTSTRUCTURE_H
#define TEXTSTRUCTURE_H
#include <stdint.h>  // For fixed size integer types
#include <stdlib.h> // malloc() etc.
#include <stdbool.h> //Easy boolean support
 /*
 * Main piece table data structure of the text editor. 
 * Some ajacent functionality is also included in this file.
 * Some of the fundamental principles were developped with regars to: https://www.cs.unm.edu/~crowley/papers/sds/sds.html
 */

typedef int ReturnCode; /* 1: success; negative: failure; 0: undefined*/
typedef int Position; /* a position in the sequence */
typedef int Size; /* a length mesurment (size == last index +1, if first index == 0) */
typedef uint8_t Atomic;/* 1 bytes (warning: is smaller then the atomic size of some utf-8 character (since up to 4 bytes for 1 utf-8 char)) */
typedef void *Item; 
typedef enum { LINUX, MSDOS, MAC, NO_INIT} LineBstd;
typedef enum { LINUX_MSDOS_ID='\n', MAC_ID='\r', NONE_ID='\0'} LineBidentifier;

/*
=========================
  Main Text-Sequence
=========================
*/
typedef struct Sequence;

/*
=========================
  Setup
=========================
*/
Sequence* Empty(LineBstd LineBstdToUse); /* Create an empty sequence (i.e. for new empty file)*/
Sequence NewSequence( char *file_name ); /* i.e. open a file */
LineBstd getCurrentLineBstd(); 
LineBidentifier getCurrentLineBidentifier(); /* returns '\n' for Linux & MSDOS or '\r' for MAC */

/*
=========================
  Quit
=========================
*/
ReturnCode SaveSequence( char *file_path, Sequence *sequence );
ReturnCode Close( Sequence *sequence, bool forceFlag ); /* Free all resources of specified sequence, forceFlag: 1 -> Close even if not saved, 0 -> Close only when saved */

/*
=========================
  Read
=========================
*/
Size getItemBlock( Sequence *sequence, Position position, Atomic **returnedItemBlock); /* More efficient when retrieving multiple consecutive Items , if multiple Items are already stored in a consecutive block. Size == last index + 1 (of return Block) or -1 to indicate error.*/

/*
=========================
  Write/Edit
=========================
*/
ReturnCode Insert( Sequence *sequence, Position position, Sequence sequenceToInsert ); /*(TODO : may reconsider how to pass the sequence to be inserted.) */
ReturnCode Delete( Sequence *sequence, Position beginPosition, Position endPosition );

ReturnCode Copy( Sequence *sequence, Position fromBegin, Position fromEnd, Position toPosition );
ReturnCode Move( Sequence *sequence, Position fromBegin, Position fromEnd, Position toPosition );

ReturnCode Undo( Sequence *sequence);
ReturnCode Redo( Sequence *sequence);
#endif