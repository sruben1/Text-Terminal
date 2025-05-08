// textStructure.c
#include "textStructure.h"  
#include <wchar.h>
#include <stdlib.h>
#include <string.h>

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
static uint8_t endOfTextSignal = END_OF_TEXT_CHAR;

/*------ Declarations ------ */
NodeResult getNodeForPosition(Sequence *sequence, Position position);
ReturnCode writeToAddBuffer(Sequence *sequence, wchar_t *textToInsert);

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
  
  // TODO: prevent splitting within a multi byte utf-8 char
  // TODO: handle cast of last descriptor node

  if(sequence != NULL){
    NodeResult nodeResult = getNodeForPosition(sequence, position);
    DescriptorNode* node = nodeResult.node;
    if (node != NULL) {
      int offset = node->offset + (position - nodeResult.startPosition);
      if (node->isInFileBuffer){
        *returnedItemBlock = sequence->fileBuffer.data + offset;
      } else {
        *returnedItemBlock = sequence->addBuffer.data + offset;
      }
      return node->size - offset;
    } 
  }

  return size;
}

/* Helper function for traversing the piece table to find the node containing text at a given position */
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
    firstNode->size = wcslen(textToInsert);
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

  // TODO: Maybe handle edge cases (e.g. inserting at the end) if necessary
  int distanceInBlock = position - nodeResult.startPosition;
  if (writeToAddBuffer(sequence, textToInsert) == -1){
    return -1; // Error: Failed to write to buffer
  }

  // First node points to the preceding text
  firstNode->size = distanceInBlock;

  // Second node points to the inserted text
  DescriptorNode* secondNode = (DescriptorNode*) malloc(sizeof(DescriptorNode));
  secondNode->isInFileBuffer = false;
  secondNode->size = wcslen(textToInsert);
  secondNode->offset = sequence->addBuffer.size - secondNode->size;

  // Third node points to the subsequent text
  DescriptorNode* thirdNode = (DescriptorNode*) malloc(sizeof(DescriptorNode));
  thirdNode->isInFileBuffer = firstNode->isInFileBuffer;
  thirdNode->offset = firstNode->offset + distanceInBlock;
  thirdNode->size = firstNode->size - distanceInBlock;

  // Update the piece table
  thirdNode->next_ptr = firstNode->next_ptr;
  secondNode->next_ptr = thirdNode;
  firstNode->next_ptr = secondNode;
  sequence->pieceTable.length += 2;

  return 1;
}

/* Appends the textToInsert to the add buffer */
ReturnCode writeToAddBuffer(Sequence *sequence, wchar_t *textToInsert) {
    if (sequence == NULL || textToInsert == NULL) {
        return -1; // Error: Invalid input
    }

    // Get the length of the text's UTF-8 representation in bytes
    size_t byteLength = wcstombs(NULL, textToInsert, 0);

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
    wcstombs(sequence->addBuffer.data + sequence->addBuffer.size, textToInsert, byteLength);
    sequence->addBuffer.size += byteLength;

    return 1;
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