#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include "textStructure.h"

LineBidentifier initSequenceFromOpenOrCreate(const char* pathname, Sequence* emptySequences, int *fileDescriptorToReturn, LineBidentifier lbStdForNewFile);

#endif