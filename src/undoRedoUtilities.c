#include "undoRedoUtilities.h"
#include "debugUtil.h"

typedef struct OperationNode OperationNode;
struct OperationNode {
    Operation *operation;
    OperationNode *next;
};

typedef struct OperationStack {
    OperationNode *top;
    int size;
} OperationStack;

/*
======================
  Internal Utilities
======================
*/

/**
 * Helper function to undo an operation.
 * This function should handle the logic of restoring the piece table
 * Due to symmetry, this function can be used for both undo and redo operations.
 * Returns the inverse operation or NULL if the operation cannot be undone.
 */
Operation* undoOperation(Sequence *sequence, Operation *operation) {
    // Get the nodes at the start and end of the operation
    DescriptorNode *first = operation->first;
    DescriptorNode *oldNext = operation->oldNext;
    DescriptorNode *last = operation->last;
    DescriptorNode *oldPrev = operation->oldPrev;
    int prevWordCount = operation->wordCount;
    int prevLineCount = operation->lineCount;
    int optimizedCase = operation->optimizedCase;
    unsigned long optimizedCaseSize = operation->optimizedCaseSize;
    free(operation); 

    sequence->lastLineResult.foundPosition = -1; // Make cache invalid

    if (optimizedCase) {
        // Handle optimized case where the operation is just an extension of a node
        if (first == NULL) {
            ERR_PRINT("Invalid operation nodes for undo in optimized case.\n");
            return NULL;
        }
        // Restore the size of the first node
        first->size -= optimizedCaseSize;

        // Create an inverse operation
        Operation *inverse = (Operation*) malloc(sizeof(Operation));
        if (inverse == NULL) {
            ERR_PRINT("Failed to allocate memory for inverse operation in optimized case.\n");
            return NULL;
        }
        inverse->first = first;
        inverse->oldNext = NULL;
        inverse->last = NULL;
        inverse->oldPrev = NULL;
        inverse->wordCount = sequence->wordCount;
        inverse->lineCount = sequence->lineCount;
        inverse->optimizedCase = 1; // Indicate this is an optimized case
        inverse->optimizedCaseSize = -optimizedCaseSize; // Negative to revert the size change
        
        // Update the sequence statistics
        sequence->wordCount = prevWordCount;
        sequence->lineCount = prevLineCount;
        
        return inverse;
    }
        
    if (first == NULL || oldNext == NULL || last == NULL || oldPrev == NULL) {
        ERR_PRINT("Invalid operation nodes for undo.\n");
        return NULL;
    }

    // Create an inverse operation
    Operation *inverse = (Operation*) malloc(sizeof(Operation));
    if (inverse == NULL) {
        ERR_PRINT("Failed to allocate memory for inverse operation.\n");
        return NULL;
    }
    inverse->first = first;
    inverse->oldNext = first->next_ptr; 
    inverse->last = last;
    inverse->oldPrev = last->prev_ptr;
    inverse->wordCount = sequence->wordCount;
    inverse->lineCount = sequence->lineCount;
    inverse->optimizedCase = 0; // Not an optimized case
    inverse->optimizedCaseSize = 0; // Not used in this case

    // Restore the piece table by reconnecting the nodes
    first->next_ptr = oldNext;
    last->prev_ptr = oldPrev;

    // Update the sequence statistics
    sequence->wordCount = prevWordCount;
    sequence->lineCount = prevLineCount;

    return inverse; 
}

/*
========================
  Undo/Redo Operations
========================
*/

ReturnCode undo(Sequence *sequence) {
    // Get undo stack and last operation
    OperationStack *undoStack = sequence->undoStack;
    if (undoStack == NULL || undoStack->size == 0) {
        ERR_PRINT("Undo stack is empty or NULL.\n");
        return 0; // Undefined
    }
    Operation *operation = popOperation(undoStack);
    if (operation == NULL) {
        ERR_PRINT("Failed to pop operation from undo stack.\n");
        return 0; // Undefined
    }

    Operation* inverse = undoOperation(sequence, operation);

    // Update the redo stack
    OperationStack *redoStack = sequence->redoStack;
    if (redoStack == NULL) {
        ERR_PRINT("Redo stack is NULL.\n");
        free(inverse);
        return 0; // Undefined
    }
    if (pushOperation(redoStack, inverse) == 0) {
        ERR_PRINT("Failed to push operation onto redo stack.\n");
        free(inverse);
        return 0; // Undefined
    }

    return 1; // Success
}

ReturnCode redo(Sequence *sequence) {
    // Get redo stack and last operation
    OperationStack *redoStack = sequence->redoStack;
    if (redoStack == NULL || redoStack->size == 0) {
        ERR_PRINT("Redo stack is empty or NULL.\n");
        return 0; // Undefined
    }
    Operation *operation = popOperation(redoStack);
    if (operation == NULL) {
        ERR_PRINT("Failed to pop operation from redo stack.\n");
        return 0; // Undefined
    }

    Operation* inverse = undoOperation(sequence, operation);

    // Update the undo stack
    OperationStack *undoStack = sequence->undoStack;
    if (undoStack == NULL) {
        ERR_PRINT("Undo stack is NULL.\n");
        free(inverse);
        return 0; // Undefined
    }
    if (pushOperation(undoStack, inverse) == 0) {
        ERR_PRINT("Failed to push operation onto undo stack.\n");
        free(inverse);
        return 0; // Undefined
    }

    return 1; // Success
}

/*
==============================
  Undo/Redo Stack Management
==============================
*/

OperationStack* createOperationStack() {
    OperationStack *stack = (OperationStack *)malloc(sizeof(OperationStack));
    if (stack == NULL) {
        ERR_PRINT("Failed to allocate memory for operation stack.\n");
        return NULL;
    }
    stack->top = NULL;
    stack->size = 0;
    return stack;
}

ReturnCode pushOperation(OperationStack *stack, Operation *operation) {
    if (stack == NULL) {
        ERR_PRINT("Cannot push operation onto a NULL stack.\n");
        return 0;
    }
    
    OperationNode *newNode = (OperationNode *)malloc(sizeof(OperationNode));
    if (newNode == NULL) {
        ERR_PRINT("Failed to allocate memory for new operation node.\n");
        return 0;
    }
    
    newNode->operation = operation;
    newNode->next = stack->top;
    stack->top = newNode;
    stack->size++;

    return 1; // Success
}

Operation* popOperation(OperationStack *stack) {
    if (stack == NULL || stack->top == NULL) {
        ERR_PRINT("Cannot pop operation from an empty or NULL stack.\n");
        return NULL;
    }
    
    OperationNode *nodeToPop = stack->top;
    Operation *operation = nodeToPop->operation;
    stack->top = nodeToPop->next;
    free(nodeToPop);
    stack->size--;
    
    return operation;
}

int getOperationStackSize(OperationStack *stack) {
    if (stack == NULL) {
        ERR_PRINT("Cannot get size of a NULL stack.\n");
        return -1; // Undefined
    }
    return stack->size;
}

ReturnCode freeOperationStack(OperationStack *stack) {
    if (stack == NULL) {
        return 0;
    }
    
    OperationNode *current = stack->top;
    while (current != NULL) {
        OperationNode *nextNode = current->next;
        free(current->operation); // Assuming operation needs to be freed
        free(current);
        current = nextNode;
    }
    
    free(stack);
    return 1; // Success
}

