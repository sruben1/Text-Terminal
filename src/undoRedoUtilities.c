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

void pushOperation(OperationStack *stack, Operation *operation) {
    if (stack == NULL) {
        ERR_PRINT("Cannot push operation onto a NULL stack.\n");
        return;
    }
    
    OperationNode *newNode = (OperationNode *)malloc(sizeof(OperationNode));
    if (newNode == NULL) {
        ERR_PRINT("Failed to allocate memory for new operation node.\n");
        return;
    }
    
    newNode->operation = operation;
    newNode->next = stack->top;
    stack->top = newNode;
    stack->size++;
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

int isOperationStackEmpty(OperationStack *stack) {
    return (stack == NULL || stack->top == NULL);
}

void freeOperationStack(OperationStack *stack) {
    if (stack == NULL) {
        return;
    }
    
    OperationNode *current = stack->top;
    while (current != NULL) {
        OperationNode *nextNode = current->next;
        free(current->operation); // Assuming operation needs to be freed
        free(current);
        current = nextNode;
    }
    
    free(stack);
}

