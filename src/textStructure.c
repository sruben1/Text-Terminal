// textStructure.c
#include "textStructure.h"  
#include <wchar.h>
#include <stdlib.h>
#include <string.h>

#include "debugUtil.h"

/*------ Data structures for internal use ------*/
typedef struct {
  DescriptorNode *node;
  Position startPosition; // position of the block's first character
} NodeResult;

/*------ Variables for internal use ------*/
static LineBstd _currLineB = NO_INIT;
static LineBidentifier _currLineBidentifier = NONE_ID;
static char currentFile[] =  {'\0'};
static bool currentlySaved = true;
static Atomic endOfTextSignal = END_OF_TEXT_CHAR;

/*------ Declarations ------ */
NodeResult getNodeForPosition(Sequence *sequence, Position position);
ReturnCode writeToAddBuffer(Sequence *sequence, wchar_t *textToInsert);
size_t getUtf8ByteSize(const wchar_t* wstr);

/*------ Function Implementations ------*/
Sequence* Empty(LineBstd LineBstdToUse){
  Sequence *newSeq = (Sequence*) malloc(sizeof(Sequence));
  newSeq->pieceTable.first = NULL;
  newSeq->pieceTable.length = 0;
  newSeq->fileBuffer.data = NULL;
  newSeq->fileBuffer.size = 0;
  newSeq->fileBuffer.capacity = 0;
  newSeq->addBuffer.data = NULL;
  newSeq->addBuffer.size = 0;
  newSeq->addBuffer.capacity = 0;
  return newSeq;
}

LineBstd getCurrentLineBstd(){
  return LINUX;
}
LineBidentifier getCurrentLineBidentifier(){
  return LINUX_MSDOS_ID;
}

Size getItemBlock( Sequence *sequence, Position position, Atomic **returnedItemBlock){
  if (sequence == NULL || position < 0 || returnedItemBlock == NULL){
    return -1; // Error
  }
  Size size = -1;
  
  NodeResult nodeResult = getNodeForPosition(sequence, position);

  if (nodeResult.startPosition == -2){
    // Special case: Position at end of the sequence requested
    *returnedItemBlock = &endOfTextSignal;
    return 1;
  }

  DescriptorNode* node = nodeResult.node;
  if (node != NULL) {
    int offset = node->offset + (position - nodeResult.startPosition);
    size = node->size - offset;

    if (node->isInFileBuffer){
      *returnedItemBlock = sequence->fileBuffer.data + offset;
    } else {
      *returnedItemBlock = sequence->addBuffer.data + offset;
    }
  } 

  return size;
}

/**
 * Helper function for traversing the piece table to find the node containing text at a given position. 
 */
NodeResult getNodeForPosition(Sequence *sequence, Position position){
  NodeResult result = {NULL, -1};

  if (sequence == NULL || position < 0){
    return result; // Error 
  }

  DescriptorNode* curr = sequence->pieceTable.first;
  int i = 0;
  while(curr != NULL){
    i += curr->size;
    if (i > position){
      result.node = curr;
      result.startPosition = i - curr->size;
      break;
    }
    curr = curr->next_ptr;
  }
  if (position == i) {
    result.startPosition = -2; // Special case: Position at end of the sequence requested
  }

  return result;
}

ReturnCode Insert( Sequence *sequence, Position position, wchar_t *textToInsert ){
  if (sequence == NULL || textToInsert == NULL){
    return -1; // Error
  }

  // If the sequence is empty, handle first insertion separately
  if (sequence->pieceTable.length == 0 && position == 0){
    if (writeToAddBuffer(sequence, textToInsert) == -1){
      return -1; // Error: Failed to write to buffer
    }
    DescriptorNode* firstNode = (DescriptorNode*) malloc(sizeof(DescriptorNode));
    firstNode->isInFileBuffer = false;
    firstNode->offset = 0;
    firstNode->size = (long int) getUtf8ByteSize(textToInsert);
    firstNode->next_ptr = NULL;

    sequence->pieceTable.first = firstNode;
    sequence->pieceTable.length = 1;

    return 1;
  }

  NodeResult nodeResult = getNodeForPosition(sequence, position);
  DescriptorNode* firstNode = nodeResult.node;
  if (firstNode == NULL){
    return -1; // Error: Position out of bounds
  }

  int distanceInBlock = position - nodeResult.startPosition;
  int newlyWrittenBufferOffset = writeToAddBuffer(sequence, textToInsert);
  if (newlyWrittenBufferOffset == -1){
    ERR_PRINT("Insert failed at write to add buffer.\n");
    return -1; // Error: Failed to write to buffer
  }

  DescriptorNode* previous = firstNode;
  DescriptorNode* current = firstNode;
  int summedSizes = 0;
  while(current != NULL){
    summedSizes += current->size;
    // Reached node of or exactly at "position": 
    if(position < summedSizes){ 
      // Undo last operation:
      summedSizes -= current->size;
      break;
    }
    previous = current;
    current = current->next_ptr; 
  }

  DescriptorNode* newInsert = (DescriptorNode*) malloc(sizeof(DescriptorNode));
  if(newInsert == NULL){
    ERR_PRINT("Fatal malloc fail at insert operation!\n");
    return -1;
  }

  // Updated newly created node with the new insert:
  newInsert->isInFileBuffer = false;
  newInsert->offset = newlyWrittenBufferOffset;
  newInsert->size = (long int) getUtf8ByteSize(textToInsert);
  
  // >> Piece Table update cases <<
  if(position == summedSizes -1){  //Case of insert at already existing piece table split:
    // Update piece table state:
    previous->next_ptr = newInsert;
    //(if end of table: current == NULL, so this behavior ok):
    newInsert->next_ptr = current;

    sequence->pieceTable.length += 1;
  } else {//Case of insert position within an existing piece:
    // Need to split into 3 pieces:
    // convert current as first part and put remaining in seccond:
    DescriptorNode* seccondPart = (DescriptorNode*) malloc(sizeof(DescriptorNode)); 
    seccondPart->size = current->size - (position - summedSizes); //set size of inverse of first part.
    current->size = position - summedSizes; //set size of first part.
    // current already at right offset since essentially truncating it.
    seccondPart->offset =  current->offset + current->size;
    
    // Update the piece table
    seccondPart->next_ptr = current->next_ptr;
    newInsert->next_ptr = seccondPart;
    current->next_ptr = newInsert;

    sequence->pieceTable.length += 2;
  }
  return 1;
}

/* Appends the textToInsert to the add buffer, returns the (offset) Position */
Position writeToAddBuffer(Sequence *sequence, wchar_t *textToInsert) {
    if (sequence == NULL || textToInsert == NULL) {
        return -1; // Error: Invalid input
    }

    // Get the length of the text's UTF-8 representation in bytes
    size_t byteLength = getUtf8ByteSize(textToInsert);

    // TODO: Maybe shrink the buffer if it unnecessarily large

    // Double the buffer's capacity if necessary
    if (sequence->addBuffer.capacity < sequence->addBuffer.size + byteLength) {
        size_t newCapacity = (sequence->addBuffer.capacity == 0) ? byteLength : sequence->addBuffer.capacity * 2;
        while (newCapacity < sequence->addBuffer.size + byteLength) {
            newCapacity *= 2;
        }

        char *newData = realloc(sequence->addBuffer.data, newCapacity);
        if (newData == NULL) {
            return -1; // Error: Memory allocation failed
        }

        sequence->addBuffer.data = newData;
        sequence->addBuffer.capacity = newCapacity;
    }

    // Write the UTF-8 string to the end of the add buffer
    int offset = (int) sequence->addBuffer.size;
    wcstombs(sequence->addBuffer.data + sequence->addBuffer.size, textToInsert, byteLength);
    sequence->addBuffer.size += byteLength;

    return (Position) offset;
}

ReturnCode Close( Sequence *sequence, bool forceFlag ){
  if(currentlySaved == false){
    return -1;
  } else if(sequence != NULL){
    DescriptorNode* curr = sequence->pieceTable.first;
    while(curr != NULL){
      DescriptorNode* next = curr->next_ptr;
      free(curr);
      curr = next;
    }
    return 1;
  }
  return -1;
}


void debugPrintInternalState(Sequence* sequence, bool showAddBuff, bool showFileBuff){
  if(sequence->addBuffer.data != NULL){
    DEBG_PRINT("AddBuff valid...\n");
    DEBG_PRINT("AddBuff size: %d\n", (int) sequence->addBuffer.size);
    DEBG_PRINT("AddBuff capacity: %d\n", (int) sequence->addBuffer.capacity);
  }
  if(sequence->fileBuffer.data != NULL){
    DEBG_PRINT("FileBuff valid...\n");
    DEBG_PRINT("FileBuff size: %d\n", (int) sequence->fileBuffer.size);
    DEBG_PRINT("FileBuff capacity: %d\n", (int) sequence->fileBuffer.capacity);
  }
  int summedPosition = 0;
  int i = 0;
  NodeResult nodeRes;
  while(i >= 0){
    nodeRes = getNodeForPosition(sequence, summedPosition);
    if(nodeRes.startPosition == -1 || nodeRes.node == NULL){
      DEBG_PRINT("Previous node likely the last one.\n");
      i = -1;
      break;
    } else{
      summedPosition += nodeRes.node->size;
      if(showAddBuff && !nodeRes.node->isInFileBuffer){
        DEBG_PRINT("Add buffer node found.\n");
        DEBG_PRINT("Offset into add buff: %ld, corresponding size: %ld .\n", nodeRes.node->offset, nodeRes.node->size);
      }
      if(showFileBuff){
        // TODO
      }
    }
  }
  if(showAddBuff && sequence->addBuffer.data != NULL){
    DEBG_PRINT("Content of add buffer:\n|");
    for(int i = 0; i < sequence->addBuffer.size; i++){
      DEBG_PRINT("%02X|", (uint8_t) sequence->addBuffer.data[i]);
    }
    DEBG_PRINT("\n\n");
  }
  if(showFileBuff && sequence->fileBuffer.data != NULL){
    DEBG_PRINT("Content of file buffer:\n|");
    for(int i = 0; i < sequence->fileBuffer.size; i++){
      DEBG_PRINT("%02X|", (uint8_t) sequence->fileBuffer.data[i]);
    }
    DEBG_PRINT("\n\n");
  }
}



size_t getUtf8ByteSize(const wchar_t* wstr) {
    mbstate_t state = {0};
    return wcsrtombs(NULL, &wstr, 0, &state);
}