// textStructure.c
#include "textStructure.h"  

/*------ Variables for internal use ------*/
static LineBstd _currLineB = NONE;
static LineBidentifier _currLineBidentifier = NONE_ID;
static char currentFile[] =  {'\0'};
static bool currentlySaved = true;

/*------ Function Implementations ------*/
Sequence* Empty(LineBstd LineBstdToUse){
  Sequence *newSeq = (Sequence*) malloc(sizeof(Sequence));
  newSeq->pieceTable.first = NULL;
  newSeq->pieceTable.length = 0;
  return newSeq;
}

LineBstd getCurrentLineBstd(){
  return LINUX;
}
LineBidentifier getCurrentLineBidentifier(){
  return LINUX_MSDOS_ID;
}

Size getItemBlock( Sequence *sequence, Position position, Item **returnedItemBlock){

  static char dummyString[] = {"Lorem ipsum dolor sit amet, \nconsectetur adipiscing elit,\n sed do eiusmod tempor incididunt ut \nlabore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation\n ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur.\n Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum ÄÖã."};

  int startPosOfBLock = 0; 
  int offset = position - startPosOfBLock;
  Size size = sizeof(dummyString) - offset;

  if(size >= 1){
    *returnedItemBlock = (Item*) ((Atomic*)&dummyString + offset);
    return size;
  } else{
    *returnedItemBlock = NULL;
    return -1;
  }
  
  /* TODO : continue implementation
  Size size = -1;

  if(sequence != NULL){
    DescriptorNode* curr = sequence->first;  
    int i = 0;
    while(curr != NULL){
      size = curr->size;
      i = i + size;
    } 
  }

  return size;
  */
}

ReturnCode Close( Sequence *sequence, bool forceFlag ){
  if(currentlySaved == false){
    return -1;
  } else if(sequence != NULL){
    struct DescriptorNode* curr = sequence->pieceTable.first;
    while(curr != NULL){
      struct DescriptorNode* next = curr->next_ptr;
      free(curr);
      curr = next;
    }
    return 1;
  }
  return -1;
}