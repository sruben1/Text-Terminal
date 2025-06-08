#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include "textStructure.h"

LineBidentifier initSequenceFromOpenOrCreate(const char* pathname, Sequence* emptySequences, LineBidentifier lbStdForNewFile);
ReturnCode saveSequenceToOpenFile(Sequence* sequence);
void closeAllFileResources(Sequence *seq);
#endif