#ifndef UNDOREDO_UTILITIES_H
#define UNDOREDO_UTILITIES_H

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

    // For optimization, some insertions simply extend a node.
    // In this case first stores the node to extend and the other nodes are NULL.
    int optimizedCase; // 1 if this is an optimized case, 0 otherwise
    unsigned long optimizedCaseSize; // Byte size of the inserted data (negative to revert)
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

#endif