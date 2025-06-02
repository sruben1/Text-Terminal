#include "textStructure.h"

typedef struct {
    DescriptorNode *first;
    DescriptorNode *last;
} Operation;

typedef struct OperationStack OperationStack;

OperationStack* createOperationStack();
void pushOperation(OperationStack *stack, Operation *operation);
Operation* popOperation(OperationStack *stack);
int isOperationStackEmpty(OperationStack *stack);
void freeOperationStack(OperationStack *stack);