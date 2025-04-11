#include <stdio.h>
#include "textStructure.h"

int main(int argc, char *argv[]){

    // Basic usage example:
    Sequence* seq = Empty(LINUX);
    printf("Created empty sequence with line break standard: %d\n", LINUX);
    
    LineBstd currentStd = getCurrentLineBstd();
    printf("Current line break standard: %d\n", currentStd);
    
    LineBidentifier currentId = getCurrentLineBidentifier();
    printf("Current line break identifier: %u\n", currentId);
    
    Item* itemBlock = NULL;
    Size blockSize = getItemBlock(seq, 0, &itemBlock);
    printf("Retrieved item block with size: %d\n", blockSize);
    printf("Retrieved item block with addr: %p\n", itemBlock);
    if (blockSize > 0 && itemBlock != NULL) {
        char* textContent = (char*)itemBlock;
        for (int i = 0; i < blockSize; i++) {
            putchar(textContent[i]);
        }
        printf("\n");
    }
    else {
        printf("Failed to retrieve item block\n");
    }

    itemBlock = NULL;
    blockSize = getItemBlock(seq, 447, &itemBlock);
    printf("Retrieved item block with size: %d\n", blockSize);
    printf("Retrieved item block with addr: %p\n", itemBlock);

    if (blockSize > 0 && itemBlock != NULL) {
        char* textContent = (char*)itemBlock;
        for (int i = 0; i < blockSize; i++) {
            putchar(textContent[i]);
        }
        printf("\n");
    }
    else {
        printf("Failed to retrieve item block\n");
    }
    
    return 0;
}