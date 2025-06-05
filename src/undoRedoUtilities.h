#ifndef UNDOREDO_UTILITIES_H
#define UNDOREDO_UTILITIES_H

#include "textStructure.h"

typedef struct Operation Operation;

/**
 * An operation is identified by the first and last nodes
 * that span the range of nodes which are affected by the operation.
 * It also stores the sequence's statistics before the operation.
 * If multiple operations should only be undone together, they can be bundled
 * by linking them together via the `previous` pointer.
 */
struct Operation {
    DescriptorNode *first;
    DescriptorNode *oldNext; // The old next node of the first node
    DescriptorNode *last;
    DescriptorNode *oldPrev; // The old previous node of the last node

    Operation *previous; // Pointer to the operation to undo after this one, NULL if this is the last operation

    int wordCount; // Word count before the operation
    int lineCount; // Line count before the operation

    // For optimization, some insertions simply extend a node.
    // In this case first stores the node to extend and the other nodes are NULL.
    int optimizedCase; // 1 if this is an optimized case, 0 otherwise
    unsigned long optimizedCaseSize; // Byte size of the inserted data (negative to revert)
};

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