#ifndef STATISTICS_H
#define STATISTICS_H

#include "textStructure.h"
#include "wchar.h"

typedef struct {
    int totalLineBreaks; // Total number of line breaks in the text
    int totalWords; // Total number of words in the text
} TextStatistics;

/**
 * Counts the number of line breaks and words caused by the data between two DescriptorNodes in a given sequence.
 * The counting starts from the startNode at startOffset and goes to the endNode at endOffset.
 * It also takes the effect for the characters on the left and right of the given span into account.
 * Words are counted based on spaces and tabs.
 * Lines are counted based on the specified line break identifier.
 */
TextStatistics calculateStatsEffect(Sequence *sequence, DescriptorNode *startNode, int startOffset, 
    DescriptorNode *endNode, int endOffset, LineBidentifier lineBreakIdentifier);

LineBidentifier findMostLikelyLineBreakStd(Sequence *sequence);

#endif