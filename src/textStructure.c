// textStructure.c
#include "textStructure.h"  

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

/*------ Function Implementations ------*/
Sequence* Empty(LineBstd LineBstdToUse){
  Sequence *newSeq = (Sequence*) malloc(sizeof(Sequence));
  newSeq->pieceTable.first = NULL;
  newSeq->pieceTable.length = 0;
  newSeq->fileBuffer.data = NULL;
  newSeq->fileBuffer.size = 0;
  newSeq->addBuffer.data = NULL;
  newSeq->addBuffer.size = 0;
  return newSeq;
}

LineBstd getCurrentLineBstd(){
  return LINUX;
}
LineBidentifier getCurrentLineBidentifier(){
  return LINUX_MSDOS_ID;
}

Size getItemBlock( Sequence *sequence, Position position, Atomic **returnedItemBlock){
  static const unsigned char dummyString[] = {// equals: "First Line\nSeccondLine \n \n\n\n\n ä🔝"
    0x46, 0x69, 0x72, 0x73, 0x74, 0x20, 0x4C, 0x69, 0x6E, 0x65, 0x0A, 
    0x53, 0x65, 0x63, 0x63, 0x6F, 0x6E, 0x64, 0x4C, 0x69, 0x6E, 0x65, 0x20, 0x0A,
    0x20, 
    0x0A, 
    0x0A, 
    0x0A, 
    0x0A, 
    0x20, 0xC3, 0xA4, 0xF0, 0x9F, 0x94, 0x9D
};

  int startPosOfBLock = 0; 
  int offset = position - startPosOfBLock;
  Size size = 36 - offset; //size == size of array

  if (position > 36){
    return -1;
  }

  if(size >= 1){
    *returnedItemBlock =  ((Atomic*)&dummyString + offset);
    return size;
  } else{
    *returnedItemBlock = NULL;
    return -1;
  }
  
  /* TODO : prevent splitting within a multi byte utf-8 char
  Size size = -1;

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
  */
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
ReturnCode writeToAddBuffer(Sequence *sequence, wchar_t *textToInsert){
  // TODO: Implement
  return -1;
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