// textStructure.c
#include "textStructure.h"  
#include <wchar.h>
#include <stdlib.h>
#include <string.h>

#include "debugUtil.h"
#include "fileManager.h" // Handles all file operations

/*------ Data structures for internal use ------*/
typedef struct {
  DescriptorNode *node;
  DescriptorNode *prevNode;
  Position startPosition; // position of the block's first character
} NodeResult;

/*------ Variables for internal use ------*/
static LineBstd _currLineB = NO_INIT;
static LineBidentifier _currLineBidentifier = NONE_ID;
static int fdOfCurrentOpenFile = NULL;
static bool currentlySaved = true;
static Atomic endOfTextSignal = END_OF_TEXT_CHAR;

/*------ Declarations ------ */
NodeResult getNodeForPosition(Sequence *sequence, Position position);
ReturnCode writeToAddBuffer(Sequence *sequence, wchar_t *textToInsert);
int isContinuationByte(Sequence *sequence, DescriptorNode *node, int offsetInBlock);
int amountOfContinuationBytes(Sequence *sequence, DescriptorNode *node, int offsetInBlock);
size_t getUtf8ByteSize(const wchar_t* wstr);

/*
=========================
  Setup
=========================
*/


LineBstd getCurrentLineBstd(){
  return LINUX;
}
LineBidentifier getCurrentLineBidentifier(){
  return LINUX_MSDOS_ID;
}

Sequence* empty(){
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

Sequence* loadOrCreateNewFile( char *pathname ){
  if(fdOfCurrentOpenFile != 0){
    ERR_PRINT("Error, Close currently open file first!\n");
    return -1;
  }
  _currLineB = initSequenceFromOpenOrCreate(pathname, empty(), &fdOfCurrentOpenFile, LINUX);

  switch (_currLineB){
    case MSDOS:
    case LINUX:
      _currLineBidentifier = LINUX_MSDOS_ID;
      break;
    case MAC:
      _currLineBidentifier = MAC_ID;
      break;
    default:
      _currLineBidentifier = NO_INIT;
      break;
  }

}

/*
=========================
  Quit
=========================
*/

ReturnCode closeSequence( Sequence *sequence, bool forceFlag ){
  if(!forceFlag && currentlySaved == false){
    return -1;
  } 
  if(sequence != NULL){
    DescriptorNode* curr = sequence->pieceTable.first;
    while(curr != NULL){
      DescriptorNode* next = curr->next_ptr;
      free(curr);
      curr = next;
    }

    //TODO : free file related resources!
    munmap(fdOfCurrentOpenFile, sequence->fileBuffer.capacity);
    close(fdOfCurrentOpenFile);
       

    free(sequence->fileBuffer.data);
    free(sequence->addBuffer.data);
    free(sequence);
    sequence == NULL;
    return 1;
  }
  return -1;
}

/*
=========================
  Internal Utilities:
=========================
*/

/**
 * Helper function for traversing the piece table to find the node containing text at a given position. 
 */
NodeResult getNodeForPosition(Sequence *sequence, Position position){
  NodeResult result = {NULL, NULL, -1};

  if (sequence == NULL || position < 0){
    return result; // Error 
  }

  DescriptorNode* curr = sequence->pieceTable.first;
  DescriptorNode* prev = NULL;
  int i = 0;
  while(curr != NULL){
    i += curr->size;
    if (i > position){
      result.node = curr;
      result.prevNode = prev;
      result.startPosition = i - curr->size;
      break;
    }
    prev = curr;
    curr = curr->next_ptr;
  }
  
  if (position == i) {
    result.prevNode = prev;
    result.startPosition = -2; // Special case: Position at end of the sequence requested
  }
  
  return result;
}

/**
 * Checks if the byte at the given offset in the node is a continuation byte.
 * A continuation byte in UTF-8 is a byte that starts with the bits 10xxxxxx.
 * Returns 1 if it is a continuation byte, 0 if not, and -1 on error.
 */
int isContinuationByte(Sequence *sequence, DescriptorNode *node, int offsetInBlock) {
  if (node == NULL || offsetInBlock < 0) {
    return -1; // Error
  }

  Atomic *data = node->isInFileBuffer ? sequence->fileBuffer.data : sequence->addBuffer.data;
  if (data == NULL) {
    return -1; // Error
  }

  Atomic byte = data[node->offset + offsetInBlock];
  return (byte & 0xC0) == 0x80; // Check if the byte is a continuation byte
}

/**
 * Reads the byte at a given offset in the node and computes the corresponding number of continuation bytes.
 * If the byte itself is a continuation byte, it determines the number of continuation bytes that follow it.
 */
int amountOfContinuationBytes(Sequence *sequence, DescriptorNode *node, int offsetInBlock) {
  if (node == NULL || offsetInBlock < 0) {
    return -1; // Error
  }

  Atomic *data = node->isInFileBuffer ? sequence->fileBuffer.data : sequence->addBuffer.data;
  if (data == NULL) {
    return -1; // Error
  }
  Atomic byte = data[node->offset + offsetInBlock];
  if (byte >= 240) { // 4-byte character (11110xxx)
    return 3;
  } else if (byte >= 224) { // 3-byte character (1110xxxx)
    return 2;
  } else if (byte >= 192) { // 2-byte character (110xxxxx)
    return 1;
  } else if (byte >= 128) { // continuation byte (10xxxxxx)
    int amount = 0;
    // Read until the end of the node or until a non-continuation byte is found
    for (int i = offsetInBlock + 1; i < node->size; i++) {
      int result = isContinuationByte(sequence, node, i);
      if (result == -1) {
        return -1; // Error
      } else if (result == 0) {
        break; // Not a continuation byte
      }     
      amount++;
    }
    return amount; 
  } else { // 1-byte character (0xxxxxxx)
    return 0; 
  }
}

size_t getUtf8ByteSize(const wchar_t* wstr) {
    mbstate_t state = {0};
    return wcsrtombs(NULL, &wstr, 0, &state);
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

/*
=========================
  Read
=========================
*/

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
    size = node->size - (position - nodeResult.startPosition);

    if (node->isInFileBuffer){
      *returnedItemBlock = sequence->fileBuffer.data + offset;
    } else {
      *returnedItemBlock = sequence->addBuffer.data + offset;
    }
  } 

  return size;
}

/*
=========================
  Write/Edit
=========================
*/

ReturnCode insert( Sequence *sequence, Position position, wchar_t *textToInsert ){
  if (sequence == NULL || textToInsert == NULL || position < 0){
    return -1; // Error
  }
  
  // Write the text to the add buffer and get the offset
  int newlyWrittenBufferOffset = writeToAddBuffer(sequence, textToInsert);
  if (newlyWrittenBufferOffset == -1){
    ERR_PRINT("Insert failed at write to add buffer.\n");
    return -1;
  }

  // Create a new node for the inserted text
  DescriptorNode* newInsert = (DescriptorNode*) malloc(sizeof(DescriptorNode));
  if(newInsert == NULL){
    ERR_PRINT("Fatal malloc fail at insert operation!\n");
    return -1;
  }
  newInsert->isInFileBuffer = false;
  newInsert->offset = newlyWrittenBufferOffset;
  newInsert->size = (long int) getUtf8ByteSize(textToInsert);

  // Separately handle insertion at the beginning of the sequence
  if (position == 0){
    DEBG_PRINT("Insert at beginning of sequence.\n");
    newInsert->next_ptr = sequence->pieceTable.first; // Potentially NULL for empty sequence
    sequence->pieceTable.first = newInsert;
    sequence->pieceTable.length += 1;
    return 1;
  }

  // Otherwise find the node for the given position
  NodeResult nodeResult = getNodeForPosition(sequence, position);

  // Insert the new node into the piece table
  if (nodeResult.startPosition == -2 || nodeResult.startPosition == position){
    // Position is at already existing piece table split
    DEBG_PRINT("Insert at already existing piece table split.\n");

    DescriptorNode* prev = nodeResult.prevNode;
    if (prev == NULL) {
      ERR_PRINT("Fatal error: prev node is NULL!\n");
      free(newInsert);
      return -1; // Error: Invalid state
    }
    prev->next_ptr = newInsert;
    newInsert->next_ptr = nodeResult.node; // also works at end of table => nodeResult.node == NULL
    sequence->pieceTable.length += 1;

  } else {
    // Position is within an existing piece
    DEBG_PRINT("Insert within an existing piece.\n");

    // Split the existing node into two parts
    DescriptorNode* firstPart = nodeResult.node;
    if (firstPart == NULL){
      ERR_PRINT("Fatal error: first part node is NULL!\n");
      free(newInsert);
      return -1;
    }
    int distanceInBlock = position - nodeResult.startPosition;
    if (isContinuationByte(sequence, firstPart, distanceInBlock) != 0){
      ERR_PRINT("Insert failed: Attempted split at continuation byte!\n");
      free(newInsert);
      return -1;
    }
    DescriptorNode* seccondPart = (DescriptorNode*) malloc(sizeof(DescriptorNode)); 
    if (seccondPart == NULL){
      ERR_PRINT("Fatal malloc fail at insert operation!\n");
      free(newInsert);
      return -1;
    }
    seccondPart->isInFileBuffer = firstPart->isInFileBuffer;
    seccondPart->size = firstPart->size - distanceInBlock; // set size as inverse of first part.
    seccondPart->offset = firstPart->offset + distanceInBlock;
    firstPart->size = distanceInBlock;
    
    // Update the piece table
    seccondPart->next_ptr = firstPart->next_ptr;
    newInsert->next_ptr = seccondPart;
    firstPart->next_ptr = newInsert;
    sequence->pieceTable.length += 2;
  }

  return 1;
}

ReturnCode delete( Sequence *sequence, Position beginPosition, Position endPosition ){
  if (sequence == NULL || beginPosition < 0 || endPosition < beginPosition) {
    return -1; // Error
  }

  // Find the nodes for the given positions and check if a deletion is possible there
  NodeResult startNodeResult = getNodeForPosition(sequence, beginPosition);
  NodeResult endNodeResult = getNodeForPosition(sequence, endPosition);
  DescriptorNode* startNode = startNodeResult.node;
  DescriptorNode* endNode = endNodeResult.node;
  if (startNode == NULL || endNode == NULL){
    ERR_PRINT("Fatal error: No node found at given position!\n");
    return -1;
  }
  int distanceInStartBlock = beginPosition - startNodeResult.startPosition;
  int distanceInEndBlock = endPosition - endNodeResult.startPosition;
  if (isContinuationByte(sequence, startNode, distanceInStartBlock) != 0 ||
      amountOfContinuationBytes(sequence, startNode, distanceInStartBlock) > endPosition - beginPosition ||
      amountOfContinuationBytes(sequence, endNode, distanceInEndBlock) > 0) {
    ERR_PRINT("Delete failed: Attempted delete at continuation byte!\n");
    return -1;
  }

  if (startNode == endNode){
    // Deleting within a single node => split it into two nodes by creating a new node at the end position
    DEBG_PRINT("Delete within a single node.\n");
    DescriptorNode* newNode = (DescriptorNode*) malloc(sizeof(DescriptorNode));
    if (newNode == NULL){
      ERR_PRINT("Fatal malloc fail at delete operation!\n");
      return -1;
    }
    newNode->isInFileBuffer = startNode->isInFileBuffer;
    newNode->offset = endNode->offset + distanceInEndBlock;
    newNode->size = endNode->size - distanceInEndBlock;
    newNode->next_ptr = startNode->next_ptr;
    distanceInEndBlock = 0;

    startNode->size = distanceInEndBlock;
    startNode->next_ptr = newNode;
    sequence->pieceTable.length += 1;
    endNode = newNode; 
  } else {
    // Deleting across multiple nodes => delete nodes in between
    DEBG_PRINT("Delete across multiple nodes.\n");
    DescriptorNode* curr = startNode->next_ptr;
    while (curr != NULL && curr != endNode) {
      DescriptorNode* next = curr->next_ptr;
      free(curr);
      curr = next;
      sequence->pieceTable.length -= 1;
    }
  }

  // Adjust the node at the end position
  if (distanceInEndBlock + 1 == endNode->size) {
    // Deletion includes last character of endNode => use next node instead
    DEBG_PRINT("Delete includes last character of endNode.\n");
    DescriptorNode* nextNode = endNode->next_ptr;
    free(endNode); 
    endNode = nextNode; 
    sequence->pieceTable.length -= 1;
  } else {
    // Deletion does not include last character of endNode
    DEBG_PRINT("Delete does not include last character of endNode.\n");
    endNode->offset += distanceInEndBlock + 1;
    endNode->size -= distanceInEndBlock + 1;
  }
  
  // Adjust the node at the start position
  if (distanceInStartBlock == 0) {
    // Deletion includes first character of startNode => use previous node instead
    DEBG_PRINT("Delete includes first character of startNode.\n");
    free(startNode);
    sequence->pieceTable.length -= 1;
    DescriptorNode* prevNode = startNodeResult.prevNode;
    if (prevNode != NULL) {
      prevNode->next_ptr = endNode;
    } else if (sequence->pieceTable.first == startNode) {
      // If startNode is the first node, update the piece table's first pointer
      sequence->pieceTable.first = endNode;
    } else {
      ERR_PRINT("Fatal error: prev node is NULL!\n");
      return -1;
    }
  } else {
    // Deletion does not include first character of startNode
    DEBG_PRINT("Delete does not include first character of startNode.\n");
    startNode->size = distanceInStartBlock;
    startNode->next_ptr = endNode;
  }
  
  return 1;
}

/*
=============
  Debug Utils
=============
*/

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
    for(int i = 0; i < (int) sequence->addBuffer.size; i++){
      DEBG_PRINT("%02X|", (uint8_t) sequence->addBuffer.data[i]);
    }
    DEBG_PRINT("\n\n");
  }
  if(showFileBuff && sequence->fileBuffer.data != NULL){
    DEBG_PRINT("Content of file buffer:\n|");
    for(int i = 0; i < (int) sequence->fileBuffer.size; i++){
      DEBG_PRINT("%02X|", (uint8_t) sequence->fileBuffer.data[i]);
    }
    DEBG_PRINT("\n\n");
  }
}