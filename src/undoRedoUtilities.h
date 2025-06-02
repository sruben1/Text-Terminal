#include "textStructure.h"

/**
 * An operation is identified by the first and last nodes
 * that span the range of nodes which are affected by the operation.
 */
typedef struct {
    DescriptorNode *first;
    DescriptorNode *oldNext; // The old next node of the first node
    DescriptorNode *last;
    DescriptorNode *oldPrev; // The old previous node of the last node
} Operation;

typedef struct OperationStack OperationStack;

/*
========================
  Undo/Redo Operations
========================
*/
ReturnCode undo(Sequence *sequence);
ReturnCode redo(Sequence *sequence);

/*
==============================
  Undo/Redo Stack Management
==============================
*/
OperationStack* createOperationStack();
ReturnCode pushOperation(OperationStack *stack, Operation *operation);
Operation* popOperation(OperationStack *stack);
int getOperationStackSize(OperationStack *stack);
ReturnCode freeOperationStack(OperationStack *stack);