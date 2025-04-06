// textStructure.h
#include <stdint.h>  // For fixed size integer types
#ifndef TEXTSTRUCTURE_H
#define TEXTSTRUCTURE_H
 /*
 * Main piece table data structure of the text editor. 
 * Some ajacent functionality is also included in this file.
 * Some of the fundamental principles were developped with regars to: https://www.cs.unm.edu/~crowley/papers/sds/sds.html
 */

typedef int ReturnCode; /* 1: success; negative: failure; 0: undefined*/
typedef int Position; /* a position in the sequence */
typedef int Size; /* a length mesurment (size == last index +1, if first index == 0) */
typedef uint8_t Atomic;/* 1 bytes (warning: is smaller then the atomic size of some utf-8 character (since up to 4 bytes for 1 utf-8 char)) */
typedef void* Item; 

typedef struct {
    /* TODO */
} Sequence;


Sequence Empty(); /* Create an empty sequence (i.e. for new empty file)*/
Sequence NewSequence( char *file_name ); /* i.e. open a file */
ReturnCode SaveSequence( char *file_name, Sequence *sequence );
ReturnCode Close( Sequence *sequence, _Bool forceFlag ); /* Free all resources of specified sequence, forceFLag: 1 -> Close even if not saved, 0 -> Close only when saved */

Size ItemAt( Sequence *sequence, Position position, Item **returnedItem ); /* Try to retrieve one Item at specific position. May return size -1 if invalid. Sicne we are implementing UTF-8, size may be 1 to 4 (1-4 bytes) total.*/
Size ItemAtBlock( Sequence *sequence, Position position, Item **returnedItemBlock); /* More efficient when retrieving multiple consecutive Items , if multiple Items are already stored in a consecutive block. Size == last index + 1 (of return Block) or -1 to indicate error.*/

ReturnCode Insert( Sequence *sequence, Position position, Sequence sequenceToInsert ); /*(TODO : may reconsider how to pass the sequence to be inserted.) */
ReturnCode Delete( Sequence *sequence, Position beginPosition, Position endPosition );

ReturnCode Copy( Sequence *sequence, Position fromBegin, Position fromEnd, Position toPosition );
ReturnCode Move( Sequence *sequence, Position fromBegin, Position fromEnd, Position toPosition );

ReturnCode Undo( Sequence *sequence);
ReturnCode Redo( Sequence *sequence);
#endif