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
 * An operation is identified by the first and last nodes
 * that span the range of nodes which are affected by the operation.
 * It also stores the sequence's statistics before the operation.
 * If multiple operations should only be undone together, they can be bundled
 * by linking them together via the `previous` pointer.
 * The memory for the operation is freed after use.
 */
Operation* undoOperation(Sequence *sequence, Operation *operation, Operation *nextInverse) {
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
        inverse->previous = nextInverse; // Link to the next inverse operation
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
    inverse->previous = nextInverse; // Link to the next inverse operation
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
    // Get undo & redo stacks
    OperationStack *undoStack = sequence->undoStack;
    if (undoStack == NULL || undoStack->size == 0) {
        ERR_PRINT("Undo stack is empty or NULL.\n");
        return 0; // Undefined
    }
    OperationStack *redoStack = sequence->redoStack;
    if (redoStack == NULL) {
        ERR_PRINT("Redo stack is NULL.\n");
        return 0; // Undefined
    }

    // Pop the last operation from the undo stack
    Operation *operation = popOperation(undoStack);
    if (operation == NULL) {
        ERR_PRINT("Failed to pop operation from undo stack.\n");
        return 0; // Undefined
    }

    Operation *previousOperation;
    Operation *inverse = NULL;
    while(operation != NULL) { // Iterate through the bundle of operations     
        previousOperation = operation->previous; // Save the next operation in the bundle

        // Undo the operation and get the inverse operation
        inverse = undoOperation(sequence, operation, inverse);
        if (inverse == NULL) {
            ERR_PRINT("Failed to undo operation.\n");
            return 0; // Undefined
        }

        // Push the inverse operation onto the redo stack
        if (pushOperation(redoStack, inverse) == 0) {
            ERR_PRINT("Failed to push operation onto redo stack.\n");
            free(inverse);
            return 0; // Undefined
        }

        operation = previousOperation; // Move to the next operation of the bundle
    }

    return 1; // Success
}

ReturnCode redo(Sequence *sequence) {
    // Get redo & undo stacks
    OperationStack *redoStack = sequence->redoStack;
    if (redoStack == NULL || redoStack->size == 0) {
        ERR_PRINT("Redo stack is empty or NULL.\n");
        return 0; // Undefined
    }
    OperationStack *undoStack = sequence->undoStack;
    if (undoStack == NULL) {
        ERR_PRINT("Undo stack is NULL.\n");
        return 0; // Undefined
    }

    // Pop the last operation from the redo stack
    Operation *operation = popOperation(redoStack);
    if (operation == NULL) {
        ERR_PRINT("Failed to pop operation from redo stack.\n");
        return 0; // Undefined
    }

    Operation *previousOperation;
    Operation *inverse = NULL;
    while(operation != NULL) { // Iterate through the bundle of operations     
        previousOperation = operation->previous; // Save the next operation in the bundle

        // Undo the operation and get the inverse operation
        inverse = undoOperation(sequence, operation, inverse);
        if (inverse == NULL) {
            ERR_PRINT("Failed to undo operation.\n");
            return 0; // Undefined
        }

        // Push the inverse operation onto the undo stack
        if (pushOperation(undoStack, inverse) == 0) {
            ERR_PRINT("Failed to push operation onto undo stack.\n");
            free(inverse);
            return 0; // Undefined
        }

        operation = previousOperation; // Move to the next operation of the bundle
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

Operation* getOperation(OperationStack *stack) {
    if (stack == NULL || stack->top == NULL) {
        ERR_PRINT("Cannot get operation from an empty or NULL stack.\n");
        return NULL;
    }
    
    return stack->top->operation; // Return the operation at the top of the stack without removing it
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

        // Free the bundle of operations
        Operation *operation = current->operation;
        while (operation != NULL) {
            Operation *previousOperation = operation->previous;
            free(operation);
            operation = previousOperation;
        }

        free(current); // Free the node itself
        current = nextNode;
    }
    
    free(stack);
    return 1; // Success
}

