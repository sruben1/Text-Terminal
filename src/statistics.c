#include "statistics.h"
#include <wchar.h>
#include "debugUtil.h"

#include "debugUtil.h"

/**
 * Counts the number of line breaks and words caused by the data between two DescriptorNodes in a given sequence.
 * The counting starts from the startNode at startOffset and goes to the endNode at endOffset.
 * It also takes the effect for the characters on the left and right of the given span into account.
 * Words are counted based on spaces and tabs.
 * Lines are counted based on the specified line break identifier.
 */
TextStatistics calculateStatsEffect(Sequence *sequence, DescriptorNode *startNode, int startOffset, 
    DescriptorNode *endNode, int endOffset, LineBidentifier lineBreakIdentifier) {

    TextStatistics stats = {0, 0}; // Empty statistics as default -> no effect
    if (sequence == NULL || startNode == NULL || endNode == NULL || startOffset < 0 || endOffset < 0) {
        ERR_PRINT("Invalid parameters for calculateStatsEffect: sequence, startNode, endNode must not be NULL and offsets must be non-negative.\n");
        return stats;
    }

    DescriptorNode *currentNode = startNode;
    int previousWasSpace = 1; // True for line breaks, spaces and tabs. Start with true to count the first word correctly.
    char *currentData; // Address of the current node's data
    int currentOffset = startOffset; // Offset within the current node's data
    
    // Iterate through the nodes from startNode to endNode and count line breaks & words
    while (currentNode != endNode->next_ptr) {
        currentData = currentNode->isInFileBuffer ? sequence->fileBuffer.data + currentNode->offset : sequence->addBuffer.data + currentNode->offset;

        // Count words & line breaks in the node's data
        int maximumOffset = currentNode == endNode ? endOffset + 1 : currentNode->size; // If it's the end node, limit to endOffset
        while (currentOffset < maximumOffset) {
            if (*(currentData + currentOffset) == lineBreakIdentifier) {
                stats.totalLineBreaks++;
                previousWasSpace = 1; // Reset for the next line
            } else if (*(currentData + currentOffset) == ' ' || *(currentData + currentOffset) == '\t') {
                previousWasSpace = 1; // Space or tab, not part of a word
            } else {
                if (previousWasSpace) {
                    stats.totalWords++; // Count a new word
                }
                previousWasSpace = 0; // Current character is part of a word
            }
            currentOffset++;
        }

        currentNode = currentNode->next_ptr; // Move to the next node
        currentOffset = 0; // Reset offset for the new node
    }

    // Check if the first character is a line break, space or tab
    char firstChar = (startNode->isInFileBuffer ? sequence->fileBuffer.data : sequence->addBuffer.data)[startNode->offset + startOffset];
    int firstIsSpace = (firstChar == lineBreakIdentifier || firstChar == ' ' || firstChar == '\t') ? 1 : 0;
    
    // Check if the character before the startOffset is a line break, space or tab
    int leftIsSpace;
    if (startOffset > 0) {
        // Look in the current node
        char leftChar = (startNode->isInFileBuffer ? sequence->fileBuffer.data : sequence->addBuffer.data)[startNode->offset + startOffset - 1];
        leftIsSpace = (leftChar == lineBreakIdentifier || leftChar == ' ' || leftChar == '\t') ? 1 : 0;
    } else {
        // Look in the previous node
        DescriptorNode *prevNode = startNode->prev_ptr;
        if (prevNode != NULL && prevNode != sequence->pieceTable.first) {
            char leftChar = (prevNode->isInFileBuffer ? sequence->fileBuffer.data : sequence->addBuffer.data)[prevNode->offset + prevNode->size - 1];
            leftIsSpace = (leftChar == lineBreakIdentifier || leftChar == ' ' || leftChar == '\t') ? 1 : 0;
        } else {
            leftIsSpace = 1; // No previous character => treat it like a space
        }
    }

    // Check if the last character is a line break, space or tab
    char lastChar = (endNode->isInFileBuffer ? sequence->fileBuffer.data : sequence->addBuffer.data)[endNode->offset + endOffset];
    int lastIsSpace = (lastChar == lineBreakIdentifier || lastChar == ' ' || lastChar == '\t') ? 1 : 0;

    // Check if the character after the endOffset is a line break, space or tab
    int rightIsSpace;
    if (endOffset < endNode->size - 1) {
        // Look in the current node
        char rightChar = (endNode->isInFileBuffer ? sequence->fileBuffer.data : sequence->addBuffer.data)[endNode->offset + endOffset + 1];
        rightIsSpace = (rightChar == lineBreakIdentifier || rightChar == ' ' || rightChar == '\t') ? 1 : 0;
    } else {
        // Look in the next node
        DescriptorNode *nextNode = endNode->next_ptr;
        if (nextNode != NULL && nextNode != sequence->pieceTable.last) {
            char rightChar = (nextNode->isInFileBuffer ? sequence->fileBuffer.data : sequence->addBuffer.data)[nextNode->offset];
            rightIsSpace = (rightChar == lineBreakIdentifier || rightChar == ' ' || rightChar == '\t') ? 1 : 0;
        } else {
            rightIsSpace = 1; // No next character => treat it like a space
        }
    }

    // Adjust the word count based on the surrounding spaces.
    // The following examples illustrate the logic (until this point totalWords == 1 because the span always contains 'b' as the only word):
    // a|b|c -> 0, a| b|c -> 1, a |b|c -> 0, a | b|c -> 0, 
    // a|b |c -> 1, a| b |c -> 2, a |b |c -> 1, a | b |c -> 1, 
    // a|b| c -> 0, a| b| c -> 1, a |b| c -> 1, a | b| c -> 1, 
    // a|b | c -> 0, a| b | c -> 1, a |b | c -> 1, a | b | c -> 1, 
    if ((leftIsSpace && !lastIsSpace && !rightIsSpace) || 
        (rightIsSpace && !leftIsSpace && !firstIsSpace) ||
        (!leftIsSpace && !firstIsSpace && !lastIsSpace && !rightIsSpace)) {
        // A word directly merges into a word on the left or right side while the other side does not introduce a new space
        stats.totalWords--;
    } else if (!leftIsSpace && firstIsSpace && lastIsSpace && !rightIsSpace) {
        // The span completely splits a word
        stats.totalWords++;
    }

    return stats;
}

    
/**
 * Finds the lineBstd to use for new opening, by checking the file buffer for most common occurrence.
 */
LineBstd findMostLikelyLineBreakStd(Sequence *sequence){
    LineBstd toReturn = NO_INIT;

    int linuxStdOccur = 0;
    int msdosStdOccur = 0;
    int macStdOccur = 0;
    //DEBG_PRINT("Line b std analysis:\n");
    // Optimize, don't need very large files...
    int max = (sequence->fileBuffer.size > 1500) ? 1500 : sequence->fileBuffer.size;
    for (int i = 0; i < max; i++){
        //DEBG_PRINT("%02x", (uint8_t) sequence->fileBuffer.data[i]);
        switch (sequence->fileBuffer.data[i]){
        case LINUX_MSDOS_ID:
            if(i > 0 && sequence->fileBuffer.data[i-1] == '\r'){
                msdosStdOccur++;
            } else{
                linuxStdOccur++;
            }
            break;

        case MAC_ID:
            //DEBG_PRINT("Mac case check.\n");
            if((i+1) == sequence->fileBuffer.size || sequence->fileBuffer.data[i+1] != '\n'){
                macStdOccur++;
            }
        }
        //DEBG_PRINT("\n");
    }
    //DEBG_PRINT("Mac occur:%d, msdos:%d, linux:%d", macStdOccur, msdosStdOccur, linuxStdOccur);
    if(linuxStdOccur > msdosStdOccur && linuxStdOccur > macStdOccur){
        DEBG_PRINT("Decided on lineBstd: LINUX.\n");
        return LINUX;
    } else if (msdosStdOccur > macStdOccur && msdosStdOccur > linuxStdOccur){
        DEBG_PRINT("Decided on lineBstd: MSDOS.\n");
        return MSDOS;
    } else if (macStdOccur > msdosStdOccur && macStdOccur > linuxStdOccur){
        DEBG_PRINT("Decided on lineBstd:MAC.\n");
        return MAC;
    } else{
        return NO_INIT;
    }
}