#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include "textStructure.h"

LineBidentifier initSequenceFromOpenOrCreate(const char* pathname, Sequence* emptySequences, int *fileDescriptorToReturn, LineBidentifier lbStdForNewFile);
ReturnCode saveSequenceToOpenFile(Sequence* sequence, int *currentFd);
void  closeAllFileResources(Sequence *seq, int fdOfCurrentOpenFile);
#endif