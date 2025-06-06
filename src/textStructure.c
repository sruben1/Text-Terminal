// textStructure.c
#include "textStructure.h"  
#include "undoRedoUtilities.h"
#include <wchar.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "debugUtil.h"
#include "fileManager.h" // Handles all file operations
#include "statistics.h" // For counting words and lines

/*------ Data structures for internal use ------*/
typedef struct {
  DescriptorNode *node;
  Position startPosition; // position of the block's first character
} NodeResult;

/*------ Variables for internal use ------*/
static LineBstd _currLineB = NO_INIT;
static LineBidentifier _currLineBidentifier = NONE_ID;
static int fdOfCurrentOpenFile = 0;
static bool currentlySaved = true;
static Atomic endOfTextSignal = END_OF_TEXT_CHAR;
static struct _lastInsert
{
  int _lastAtomicPos;
  int _lastCharSize;
  long int _lastWritePos;
} _lastInsert ={-1,-1, -1};


/*------ Declarations ------ */
NodeResult getNodeForPosition(Sequence *sequence, Position position);
ReturnCode writeToAddBuffer(Sequence *sequence, wchar_t *textToInsert, int *sizeOfCharOrNull);
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
  if(newSeq == NULL){
    ERR_PRINT("Fatal malloc fail at empty sequence creation!\n");
    return NULL; // Error
  }

  // Initialize undo and redo stacks
  newSeq->undoStack = createOperationStack();
  newSeq->redoStack = createOperationStack();
  if(newSeq->undoStack == NULL || newSeq->redoStack == NULL){
    ERR_PRINT("Fatal malloc fail at empty sequence creation!\n");
    free(newSeq);
    return NULL;
  }
  newSeq->wordCount = 0;
  newSeq->lineCount = 0;

  // Create sentinel nodes for the piece table
  DescriptorNode* firstNode = (DescriptorNode*) malloc(sizeof(DescriptorNode));
  DescriptorNode* lastNode = (DescriptorNode*) malloc(sizeof(DescriptorNode));
  if(firstNode == NULL || lastNode == NULL){
    ERR_PRINT("Fatal malloc fail at empty sequence creation!\n");
    freeOperationStack(newSeq->undoStack);
    freeOperationStack(newSeq->redoStack);
    free(newSeq);
    free(firstNode);
    free(lastNode);
    return NULL;
  }
  // Set values for the sentinel nodes
  firstNode->next_ptr = lastNode;
  firstNode->prev_ptr = NULL;
  firstNode->isInFileBuffer = false;
  firstNode->offset = 0;
  firstNode->size = 0;
  lastNode->next_ptr = NULL;
  lastNode->prev_ptr = firstNode;
  lastNode->isInFileBuffer = false;
  lastNode->offset = 0;
  lastNode->size = 0;

  // Set values for the piece table and buffers
  newSeq->pieceTable.first = firstNode;
  newSeq->pieceTable.last = lastNode;
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
    return NULL;
  }
  // _currLineB = initSequenceFromOpenOrCreate(pathname, empty(), &fdOfCurrentOpenFile, LINUX);

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
  freeOperationStack(sequence->undoStack);
  freeOperationStack(sequence->redoStack);
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
    // munmap(fdOfCurrentOpenFile, sequence->fileBuffer.capacity);
    // close(fdOfCurrentOpenFile);
    
    _currLineB = NO_INIT;
    _currLineBidentifier = NONE_ID;
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
  NodeResult result = {NULL, -1};

  if (sequence == NULL || position < 0){
    return result; // Error 
  }

  DescriptorNode* curr = sequence->pieceTable.first->next_ptr; // Skip sentinel node
  int i = 0;
  while(curr != sequence->pieceTable.last){
    i += curr->size;
    if (i > position){
      result.node = curr;
      result.startPosition = i - curr->size;
      break;
    }
    curr = curr->next_ptr;
  }
  
  if (position == i) {
    result.node = sequence->pieceTable.last; // Special case: Position at end of the sequence requested
    result.startPosition = i;
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
Position writeToAddBuffer(Sequence *sequence, wchar_t *textToInsert, int *sizeOfCharOrNull) {
    if (sequence == NULL || textToInsert == NULL) {
        return -1; // Error: Invalid input
    }

    // Get the length of the text's UTF-8 representation in bytes
    size_t byteLength = getUtf8ByteSize(textToInsert);
    
    // If call requested this stat, give it here...
    if (sizeOfCharOrNull != NULL){
      *sizeOfCharOrNull = byteLength;
    }

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

  if (nodeResult.node == sequence->pieceTable.last) {
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
  Query internals
=========================
*/
int getCurrentWordCount(Sequence *sequence) {
  return sequence != NULL ? sequence->wordCount : -1; // Return -1 if sequence is NULL
}

int getCurrentLineCount(Sequence *sequence) {
  return sequence != NULL ? sequence->lineCount : -1; // Return -1 if sequence is NULL
}

/**
 * Return Atomic position starting form position 1
 */
int backTrackToFirstAtomicInLine(Sequence *sequence, Atomic fromAtomic){
  // Trivial cases:
  if(fromAtomic <= 0 || fromAtomic-1 == 0){
    return 0;
  }
  
}

/**
 * Query the total amount of atomics used in current sequence state, from both the add and the file buffer. 
 */
int getCurrentTotalSize(Sequence *sequence){
  NodeResult result = {NULL, -1};

  if (sequence == NULL){
    return -1; // Error 
  }

  DescriptorNode* curr = sequence->pieceTable.first->next_ptr; // Skip sentinel node
  int i = 0;
  while(curr != sequence->pieceTable.last){
    i += curr->size;
    curr = curr->next_ptr;
  }

  return i;
}

/*
=========================
  Write/Edit
=========================
*/

ReturnCode insert( Sequence *sequence, Position position, wchar_t *textToInsert ){
  DEBG_PRINT("[Trace]: Inserting at atomic:%d\n",position);
  if (sequence == NULL || textToInsert == NULL || position < 0){
    return -1; // Error
  }

  int atomicSizeOfInsertion = -1;
  
  // Write the text to the add buffer and get the offset
  int newlyWrittenBufferOffset = writeToAddBuffer(sequence, textToInsert, &atomicSizeOfInsertion);
  if (newlyWrittenBufferOffset == -1){
    ERR_PRINT("Insert failed at write to add buffer.\n");
    return -1;
  } 

  // Store previous statistics for undo
  int prevWordCount = sequence->wordCount;
  int prevLineCount = sequence->lineCount;

  // Increase the line count if the sequence was empty
  if (position == 0 && sequence->pieceTable.first->next_ptr == sequence->pieceTable.last) {
    sequence->lineCount++;
  }

  // Optimization case: if insert at last insert (and buffer) +1 position simply extend node to hold new insert as well.
  DEBG_PRINT("cond1:%d , cond2: %d",(position == _lastInsert._lastAtomicPos + _lastInsert._lastCharSize), (newlyWrittenBufferOffset == _lastInsert._lastWritePos +  _lastInsert._lastCharSize));
  if( (position == _lastInsert._lastAtomicPos + _lastInsert._lastCharSize) && (newlyWrittenBufferOffset == _lastInsert._lastWritePos +  _lastInsert._lastCharSize)){
    NodeResult toExtend = getNodeForPosition(sequence, position-1);

    // Ensure not operating in invalid node...
    if (toExtend.node != NULL){
      DEBG_PRINT("Insert now in optimized case.\n");
      // Simply increase the valid range of the node to now also encompass the new insertion as well:
      unsigned long byteSize = (unsigned long) getUtf8ByteSize(textToInsert);
      toExtend.node->size += byteSize;

      // Update statistics
      TextStatistics stats = calculateStatsEffect(sequence, toExtend.node, toExtend.node->size - byteSize, toExtend.node, toExtend.node->size - 1, getCurrentLineBidentifier());
      sequence->wordCount += stats.totalWords;
      sequence->lineCount += stats.totalLineBreaks;

      // Save the operation for undo
      Operation *operation = (Operation*) malloc(sizeof(Operation));
      if (operation == NULL) {
        ERR_PRINT("Fatal malloc fail at insert operation!\n");
        return -1; // Error
      }
      operation->first = toExtend.node;
      operation->optimizedCase = 1;
      operation->optimizedCaseSize = byteSize;
      operation->wordCount = prevWordCount;
      operation->lineCount = prevLineCount;
      operation->oldNext = NULL; // Not used in optimized case
      operation->last = NULL;
      operation->oldPrev = NULL; 
      if (pushOperation(sequence->undoStack, operation) == 0) {
        ERR_PRINT("Failed to push operation onto undo stack.\n");
        free(operation);
        return -1; // Error
      }

      // Save this insert's properties for comparison at next insert.
      _lastInsert._lastAtomicPos = position;
      _lastInsert._lastWritePos = newlyWrittenBufferOffset;
      _lastInsert._lastCharSize = atomicSizeOfInsertion;
      return 1;
    }
  }
  // Save this insert's properties for comparison at next insert.
  DEBG_PRINT("Insert saved with size!%d, buffOfs:%d, atomPos:%d.\n", atomicSizeOfInsertion, newlyWrittenBufferOffset, position);
  _lastInsert._lastAtomicPos = position;
  _lastInsert._lastWritePos = newlyWrittenBufferOffset;
  _lastInsert._lastCharSize = atomicSizeOfInsertion;


  // Otherwise create a new node for the inserted text
  DescriptorNode* newInsert = (DescriptorNode*) malloc(sizeof(DescriptorNode));
  if(newInsert == NULL){
    ERR_PRINT("Fatal malloc fail at insert operation!\n");
    return -1;
  }
  newInsert->isInFileBuffer = false;
  newInsert->offset = newlyWrittenBufferOffset;
  newInsert->size = (long int) getUtf8ByteSize(textToInsert);

  // Find the node for the given position
  NodeResult nodeResult = getNodeForPosition(sequence, position);
  DEBG_PRINT("Node for position %d found at start position %d.\n", position, nodeResult.startPosition);

  // Insert the new node into the piece table
  if (nodeResult.startPosition == position){
    // Position is at already existing piece table split
    DEBG_PRINT("Insert at already existing piece table split.\n");

    DescriptorNode* next = nodeResult.node;
    if (next == NULL) {
      ERR_PRINT("Fatal error: next node is NULL!\n");
      free(newInsert);
      return -1; // Error: Invalid state
    }
    DescriptorNode* prev = next->prev_ptr;
    if (prev == NULL) {
      ERR_PRINT("Fatal error: previous node is NULL!\n");
      free(newInsert);
      return -1; // Error: Invalid state
    }

    // Save state for undo
    Operation *operation = (Operation*) malloc(sizeof(Operation));
    if (operation == NULL) {
      ERR_PRINT("Fatal malloc fail at insert operation!\n");
      free(newInsert);
      return -1; // Error
    }
    operation->first = prev;
    operation->oldNext = next;
    operation->last = next;
    operation->oldPrev = prev;
    operation->wordCount = prevWordCount;
    operation->lineCount = prevLineCount;
    operation->optimizedCase = 0; // Not an optimized case
    operation->optimizedCaseSize = 0; // Not used in this case
    if (pushOperation(sequence->undoStack, operation) == 0) {
      ERR_PRINT("Failed to push operation onto undo stack.\n");
      free(newInsert);
      free(operation);
      return -1; // Error
    }

    // Reset the redo stack
    freeOperationStack(sequence->redoStack);
    sequence->redoStack = createOperationStack();

    // Update the piece table
    prev->next_ptr = newInsert;
    newInsert->next_ptr = next;
    newInsert->prev_ptr = prev;
    next->prev_ptr = newInsert;

  } else {
    // Position is within an existing piece
    DEBG_PRINT("Insert within an existing piece.\n");

    // Split the existing node into two parts
    DescriptorNode* foundNode = nodeResult.node;
    if (foundNode == NULL){
      ERR_PRINT("Fatal error: found node is NULL!\n");
      free(newInsert);
      return -1; // Error: Invalid state
    }
    int distanceInBlock = position - nodeResult.startPosition;
    if (isContinuationByte(sequence, foundNode, distanceInBlock) != 0){
      ERR_PRINT("Insert failed: Attempted split at continuation byte!\n");
      free(newInsert);
      return -1;
    }
    DescriptorNode* firstPart = (DescriptorNode*) malloc(sizeof(DescriptorNode)); 
    DescriptorNode* seccondPart = (DescriptorNode*) malloc(sizeof(DescriptorNode)); 
    if (firstPart == NULL || seccondPart == NULL){
      ERR_PRINT("Fatal malloc fail at insert operation!\n");
      free(newInsert);
      return -1;
    }

    // Save the original node for undo
    Operation *operation = (Operation*) malloc(sizeof(Operation));
    if (operation == NULL) {
      ERR_PRINT("Fatal malloc fail at insert operation!\n");
      free(newInsert);
      free(firstPart);
      free(seccondPart);
      return -1;
    }
    operation->first = foundNode->prev_ptr;
    operation->oldNext = foundNode;
    operation->last = foundNode->next_ptr;
    operation->oldPrev = foundNode;
    operation->wordCount = prevWordCount;
    operation->lineCount = prevLineCount;
    operation->optimizedCase = 0; // Not an optimized case
    operation->optimizedCaseSize = 0; // Not used in this case
    if (pushOperation(sequence->undoStack, operation) == 0) {
      ERR_PRINT("Failed to push operation onto undo stack.\n");
      free(newInsert);
      free(firstPart);
      free(seccondPart);
      free(operation);
      return -1; // Error
    }

    // Reset the redo stack
    freeOperationStack(sequence->redoStack);
    sequence->redoStack = createOperationStack();

    // Set values for the new nodes
    firstPart->isInFileBuffer = foundNode->isInFileBuffer;
    firstPart->offset = foundNode->offset;
    firstPart->size = distanceInBlock;
    seccondPart->isInFileBuffer = foundNode->isInFileBuffer;
    seccondPart->size = foundNode->size - distanceInBlock; // set size as inverse of first part.
    seccondPart->offset = foundNode->offset + distanceInBlock;
    
    // Update the piece table
    firstPart->next_ptr = newInsert;
    firstPart->prev_ptr = foundNode->prev_ptr;
    foundNode->prev_ptr->next_ptr = firstPart;
    newInsert->next_ptr = seccondPart;
    newInsert->prev_ptr = firstPart;
    seccondPart->next_ptr = foundNode->next_ptr;
    seccondPart->prev_ptr = newInsert;
    foundNode->next_ptr->prev_ptr = seccondPart;
  }

  // Update statistics
  TextStatistics stats = calculateStatsEffect(sequence, newInsert, 0, newInsert, newInsert->size - 1, getCurrentLineBidentifier());
  sequence->wordCount += stats.totalWords;
  sequence->lineCount += stats.totalLineBreaks;

  return 1;
}

ReturnCode delete( Sequence *sequence, Position beginPosition, Position endPosition ){
  if (sequence == NULL || beginPosition < 0 || endPosition < beginPosition) {
    return -1; // Error
    ERR_PRINT("Delete failed at security check.\n");
  }

  // Find the nodes for the given positions and check if a deletion is possible there
  NodeResult startNodeResult = getNodeForPosition(sequence, beginPosition);
  NodeResult endNodeResult = getNodeForPosition(sequence, endPosition);
  DescriptorNode* startNode = startNodeResult.node;
  DescriptorNode* endNode = endNodeResult.node;
  if (startNode == NULL || startNode == sequence->pieceTable.last 
    || endNode == NULL || endNode == sequence->pieceTable.last) {
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

  // Get the statistics effect of the deletion
  TextStatistics stats = calculateStatsEffect(sequence, startNode, distanceInStartBlock, endNode, distanceInEndBlock, getCurrentLineBidentifier());

  // Save the original nodes for undo
  Operation *operation = (Operation*) malloc(sizeof(Operation));
  if (operation == NULL) {
    ERR_PRINT("Fatal malloc fail at delete operation!\n");
    return -1; // Error
  }
  operation->first = startNode->prev_ptr;
  operation->oldNext = startNode;
  operation->last = endNode->next_ptr;
  operation->oldPrev = endNode;
  operation->wordCount = sequence->wordCount;
  operation->lineCount = sequence->lineCount;
  operation->optimizedCase = 0; // Not an optimized case
  operation->optimizedCaseSize = 0; // Not used in this case
  if (pushOperation(sequence->undoStack, operation) == 0) {
    ERR_PRINT("Failed to push operation onto undo stack.\n");
    free(operation);
    return -1; // Error
  }

  // Reset the redo stack
  freeOperationStack(sequence->redoStack);
  sequence->redoStack = createOperationStack();

  // Determine the new start node after deletion
  DescriptorNode* newStartNode;
  if (distanceInStartBlock == 0) { 
    // Deletion includes the first character of the start node => use previous node instead
    newStartNode = startNode->prev_ptr;
  } else {
    // Create a new node which includes everything up to the beginning of the deletion
    newStartNode = (DescriptorNode*) malloc(sizeof(DescriptorNode));
    if (newStartNode == NULL) {
      ERR_PRINT("Fatal malloc fail at delete operation!\n");
      return -1;
    }
    newStartNode->isInFileBuffer = startNode->isInFileBuffer;
    newStartNode->offset = startNode->offset;
    newStartNode->size = distanceInStartBlock;
    newStartNode->prev_ptr = startNode->prev_ptr;
    startNode->prev_ptr->next_ptr = newStartNode;
    startNode = newStartNode; 
  }
  
  // Determine the new end node after deletion
  DescriptorNode* newEndNode;
  if (distanceInEndBlock + 1 == endNode->size) { 
    // Deletion includes the last character of the end node => use next node instead
    newEndNode = endNode->next_ptr;
  } else {
    // Create a new node which includes everything after the end of the deletion
    newEndNode = (DescriptorNode*) malloc(sizeof(DescriptorNode));
    if (newEndNode == NULL) {
      ERR_PRINT("Fatal malloc fail at delete operation!\n");
      return -1;
    }
    newEndNode->isInFileBuffer = endNode->isInFileBuffer;
    newEndNode->offset = endNode->offset + distanceInEndBlock + 1;
    newEndNode->size = endNode->size - distanceInEndBlock - 1;
    newEndNode->next_ptr = endNode->next_ptr;
    endNode->next_ptr->prev_ptr = newEndNode;
  }

  newStartNode->next_ptr = newEndNode; 
  newEndNode->prev_ptr = newStartNode;

  // Update statistics
  sequence->wordCount -= stats.totalWords;
  sequence->lineCount -= stats.totalLineBreaks;
  if (beginPosition == 0 && sequence->pieceTable.first->next_ptr == sequence->pieceTable.last) {
    // Deletion was the only content in the sequence, reset line count
    sequence->lineCount = 0;
  }
  
  return 1;
}

/*
=============
  Debug Utils
=============
*/

void debugPrintInternalState(Sequence* sequence, bool showAddBuff, bool showFileBuff){
  DEBG_PRINT("--- INTERNAL STATE OF SEQUENCE ---\n");
  if (sequence->addBuffer.data != NULL){
    DEBG_PRINT("Add buffer valid. Size: %d, Capacity: %d.\n", (int) sequence->addBuffer.size, (int) sequence->addBuffer.capacity);
  } else {
    DEBG_PRINT("Add buffer is NULL.\n");
  }
  if (sequence->fileBuffer.data != NULL){
    DEBG_PRINT("File buffer valid. Size: %d, Capacity: %d.\n", (int) sequence->fileBuffer.size, (int) sequence->fileBuffer.capacity);
  } else {
    DEBG_PRINT("File buffer is NULL.\n");
  }
  DEBG_PRINT("Undo stack size: %d, Redo stack size: %d.\n", getOperationStackSize(sequence->undoStack), getOperationStackSize(sequence->redoStack));

  DEBG_PRINT("--- Piece Table ---\n");
  int summedPosition = 0;
  DescriptorNode* curr = sequence->pieceTable.first;
  while (curr != NULL) {
      summedPosition += curr->size;
      if(showAddBuff && !curr->isInFileBuffer){
        DEBG_PRINT("Offset into add buff: %ld, corresponding size: %ld.\n", curr->offset, curr->size);
      }
      if(showFileBuff){
        // TODO
      }
      curr = curr->next_ptr;
  }
  if(showAddBuff && sequence->addBuffer.data != NULL){
    DEBG_PRINT("--- Content of add buffer ---\n|");
    for(int i = 0; i < (int) sequence->addBuffer.size; i++){
      DEBG_PRINT("%02X|", (uint8_t) sequence->addBuffer.data[i]);
    }
    DEBG_PRINT("\n");
  }
  if(showFileBuff && sequence->fileBuffer.data != NULL){
    DEBG_PRINT("--- Content of file buffer ---\n|");
    for(int i = 0; i < (int) sequence->fileBuffer.size; i++){
      DEBG_PRINT("%02X|", (uint8_t) sequence->fileBuffer.data[i]);
    }
    DEBG_PRINT("\n");
  }
  DEBG_PRINT("----------------------------------\n");
}