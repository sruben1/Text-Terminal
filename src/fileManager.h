#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include "textStructure.h"

LineBstd initSequenceFromOpenOrCreate(const char* pathname, Sequence* emptySequences, LineBstd lbStdForNewFile);
ReturnCode saveSequenceToOpenFile(Sequence* sequence);
void closeAllFileResources(Sequence *seq);
#endif