#include "fileManager.h"

#define _GNU_SOURCE
#include <fcntl.h> // File controls
#include <sys/stat.h> // To query file statistics
#include <unistd.h> // misc. functions related t o system and I/O
#include <sys/mman.h> // mempry map support
#include <errno.h> // Error managment
#include <signal.h> // Errors & signaling
#include <sys/types.h> // Error handling & analysis tools
#include <string.h> 

#include "debugUtil.h"
#include "statistics.h"

  // Very helpful resources:
  //(SIGBUS) mmap c style Error handling:
  //https://gist.github.com/FauxFaux/9032a665250fbcbe6fe6060ebe93a14f
  //MMAP:
  //https://comp.anu.edu.au/courses/comp2310/labs/05-malloc/
  //https://comp.anu.edu.au/courses/comp2310/labs/07-mmio/#the-mmap-system-call
  //cpp though: https://www.sublimetext.com/blog/articles/use-mmap-with-care

//internal backup to detach sequence and save file + have some recover capability 
int _internalOriginalFileCopyFd = -1;
int _internalSaveAndWriteFd = -1;
Buffer _internalSaveAndWriteMMAP = {NULL, 0, 0};


// Forward declarations
/*---- Utilities ----*/
ReturnCode writeSequenceToMapping(Atomic* writeMapping, size_t newSize, size_t newAlignedSize, const Sequence* seq);
ReturnCode resizeFileAndMapping(int fd, void** mapping, size_t currentSize, size_t currentAlignedSize, size_t newSize, size_t newAlignedSize);
void handler(int sig, siginfo_t *info, void *ucontext);
ReturnCode replaceFileBufferInSeq(int *fd, size_t fileSize, Sequence *seq);


LineBidentifier initSequenceFromOpenOrCreate(const char* pathname, Sequence* emptySequences, int *fileDescriptorToReturn, LineBidentifier lbStdForNewFile){
  if ( (*fileDescriptorToReturn = open(pathname, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR /*user has Read&Write permissions*/)) < 0){ // O_RDWR flag for R&W open; O_RDONLY flag for read only
    ERR_PRINT("Failed to open file: %s!\n", strerror(errno));
    *fileDescriptorToReturn = -1;
    return NO_INIT;
  }

  //SIGBUS mmap usage error handler setup:
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_flags = SA_SIGINFO;
  sigemptyset(&sa.sa_mask);
  sa.sa_sigaction = &handler;
  if(sigaction(SIGBUS, &sa, NULL) < 0){
    ERR_PRINT("SIGBUS handler setup failed.\n");
  }

  /*>File analysis<*/
  // get size:
  struct stat buffer;
  if(fstat(*fileDescriptorToReturn, &buffer) < 0){
    ERR_PRINT("Failed to read stats form file: %s\n", strerror(errno));
    return -1;
  }
  size_t fileSize = (size_t) buffer.st_size;

  if (fileSize > 0){
    if(replaceFileBufferInSeq(fileDescriptorToReturn, fileSize, emptySequences) < 0){
      ERR_PRINT("Failed to init MMAP into sequence structure.\n");
      return -1;
    }
    DEBG_PRINT("MMAP done moving to next steps.\n");
    _internalSaveAndWriteFd = *fileDescriptorToReturn;

    lbStdForNewFile = findMostLikelyLineBreakStd(emptySequences);
  } else{
    /*nothing to do since empty() already took care of null assignments to file buffer*/
  }

  return lbStdForNewFile;
}

/**
 * Save sequence to file efficiently using memory mappings and backup to temporary files in between
 */
ReturnCode saveSequenceToOpenFile(Sequence* sequence, int *currentFd) {
  if ( currentFd <= 0 || !sequence) {
    ERR_PRINT("Invalid parameters to saveSequenceToFile\n");
    return -1;
  }
  
  // Calculate required size (size for current sequence state)
  size_t requiredSize = getCurrentTotalSize(sequence);
  if (requiredSize <= 0) {
    ERR_PRINT("Empty sequence or calculation failed\n");
    return -1;
  }
  
  // Get current file size
  struct stat fileStat;
  if (fstat(*currentFd, &fileStat) < 0) {
    ERR_PRINT("Failed to get file stats: %s\n", strerror(errno));
    return -1;
  }
  
  /*>Special copies and preparations<*/

  // Perform file Copy of file in it`s original state to keep sequence consistent
  if (_internalOriginalFileCopyFd == -1){
    DEBG_PRINT("Started needed orig copy...\n");
    char backupPath[] = "/tmp/.TxTinternal-OrigState-XXXXXX";
    _internalOriginalFileCopyFd = mkstemp(backupPath);
    if (_internalOriginalFileCopyFd < 0) {
      ERR_PRINT("Failed to create copy of original file, aborting save: %s\n", strerror(errno));
      return -1;
    }

    off_t offset = 0;
    ssize_t copied = copy_file_range(currentFd, &offset, _internalOriginalFileCopyFd, NULL, fileStat.st_size, 0);
    if (copied != fileStat.st_size) {
      ERR_PRINT("Failed to create complete copy of original, aborting backup and save: %s\n", strerror(errno));
      close(_internalOriginalFileCopyFd);
      exit(-1);
    }

    //Update state to keep track correctly of write and sequence file...
    _internalSaveAndWriteMMAP.capacity = sequence->fileBuffer.capacity;
    _internalSaveAndWriteMMAP.data = sequence->fileBuffer.data;
    _internalSaveAndWriteMMAP.size = sequence->fileBuffer.size;
    
    //Replace internal mapping to map to original state
    if(replaceFileBufferInSeq(_internalOriginalFileCopyFd,copied, sequence) < 0){
      ERR_PRINT("Failed to replace MMAP into sequence structure.\n");
      return -1;
    }

    DEBG_PRINT("Ended needed orig copy...\n");
  }
  // Perform file backup before save
  int backupFd = -1;
  if (fileStat.st_size > 0) {
    // Create temporary backup using copy_file_range (Linux-specific, efficient)
    char backupPath[] = "/tmp/.TxTinternal-filebackup-XXXXXX";
    backupFd = mkstemp(backupPath);
    if (backupFd < 0) {
      ERR_PRINT("Failed to create backup file, aborting save: %s\n", strerror(errno));
      return -1;
    }
    
    DEBG_PRINT("Starting needed temp copy of actual file...\n");
    // Copy save file to backup using copy_file_range (zero-copy on same filesystem)
    off_t offset = 0;
    ssize_t copied = copy_file_range(*currentFd, &offset, backupFd, NULL, fileStat.st_size, 0);
    if (copied != fileStat.st_size) {
      ERR_PRINT("Failed to create complete backup, aborting backup and save: %s\n", strerror(errno));
      close(backupFd);
      return -1;
    }
    DEBG_PRINT("Ended needed temp copy...\n");
  }
  
  // Calculate aligned mapping size
  const size_t mask = (size_t) sysconf(_SC_PAGESIZE) - 1;
  size_t newAlignedSize = (requiredSize + mask) & ~mask; 
  
  Buffer* writeMapping = &_internalSaveAndWriteMMAP;

  if(writeMapping->size == requiredSize && writeMapping->capacity == newAlignedSize){
    //Nothing to do
  } else {
    resizeFileAndMapping(currentFd, writeMapping->data, sequence->fileBuffer.size, sequence->fileBuffer.capacity, requiredSize, newAlignedSize);
  }
  
  
  if (writeMapping->data == MAP_FAILED) {
    ERR_PRINT("Failed to create updated write mapping, saving sequence to other backup: %s\n", strerror(errno));
    if (backupFd >= 0) {
      // Could add sequence backup procedure...
      close(backupFd);
    }
    return -1;
  }
  
  // Write sequence data to mapped memory
  if (writeSequenceToMapping(writeMapping->data, requiredSize, newAlignedSize, sequence) < 0) {
    // Ensure data is written to disk
    if (msync(writeMapping->data, newAlignedSize, MS_SYNC) < 0) {
      ERR_PRINT("Failed to sync mapped memory, look in temp files to recover file backup: %s\n", strerror(errno));
      fprintf(stderr, "Failed to write to file, look in temp files `.TxTinternal-filebackup-...` to recover file backup.\n");
      return -1;
    }
  }

  if (backupFd >= 0) {
      close(backupFd);
    }
  
  DEBG_PRINT("File save operation likely succeeded!\n");
  return 1;
}

/**
 * Write sequence data to the mapped memory
 */
ReturnCode writeSequenceToMapping(Atomic* writeMapping, size_t newSize, size_t newAlignedSize, const Sequence* seq) {

  if (writeMapping < 0 || writeMapping != MAP_FAILED || seq != NULL) {
    return -1;
  }

  int size = 0;
  int blockOffset = 0;
  int atomicsCount = 0;
  int rollingAtomicCount = 0;
  Atomic *currentItemBlock = NULL;

  while (atomicsCount < newSize){
    DEBG_PRINT("rollingAtmcCount:%d, blockOffs:%d, size:%d.\n", rollingAtomicCount, blockOffset, size);
    if(rollingAtomicCount >= size){
      blockOffset = blockOffset + rollingAtomicCount;
      atomicsCount += rollingAtomicCount; 
      rollingAtomicCount = 0;
      DEBG_PRINT("Requesting next block in seek, at atomic:%d\n", 0 + blockOffset);
      size = getItemBlock(seq, 0 + blockOffset, &currentItemBlock);
      DEBG_PRINT("New blockOffset=%d, size=%d\n", blockOffset, size);
      if(size <= 0 || currentItemBlock[0] != END_OF_TEXT_CHAR){
        ERR_PRINT("Position at end of file or error: curr block start:%d\n", 0 + blockOffset);
        return -1;
      }
    }

    // // Copy block data
    // memcpy(writePtr, source, sizeToWrite);
    // writePtr += sizeToWrite;
    // rollingAtomicCount += sizeToWrite;
  }


  return 1;
}

/**
 * Efficient file replacement using Linux-specific features
 * This creates a new file, writes the data, then atomically replaces the original
 */
ReturnCode replaceFileContentEfficiently(const char* filepath, Sequence* sequence, LineBidentifier lineBreakStd) {
  /*
  if (!filepath || !sequence) {
    ERR_PRINT("Invalid parameters to replaceFileContentEfficiently\n");
    return -1;
  }
  
  // Create temporary file in same directory for atomic replacement
  size_t pathLen = strlen(filepath);
  char* tempPath = malloc(pathLen + 20);
  if (!tempPath) {
    ERR_PRINT("Memory allocation failed\n");
    return -1;
  }
  
  snprintf(tempPath, pathLen + 20, "%s.tmp.XXXXXX", filepath);
  
  // Create temporary file
  int tempFd = mkstemp(tempPath);
  if (tempFd < 0) {
    ERR_PRINT("Failed to create temporary file: %s\n", strerror(errno));
    free(tempPath);
    return -1;
  }
  
  // Write sequence to temporary file
  ReturnCode result = saveSequenceToFile(&tempFd, sequence, lineBreakStd);
  
  if (result == 1) {
    // Ensure all data is written
    if (fsync(tempFd) < 0) {
      ERR_PRINT("Failed to sync temporary file: %s\n", strerror(errno));
      result = -1;
    }
  }
  
  close(tempFd);
  
  if (result == 1) {
    // Atomically replace original file
    if (rename(tempPath, filepath) < 0) {
      ERR_PRINT("Failed to replace original file: %s\n", strerror(errno));
      result = -1;
    }
  }
  
  // Cleanup temporary file if replacement failed
  if (result != 1) {
    unlink(tempPath);
  }
  
  free(tempPath);
  return result;
  */
}

/*
==============
Internal Utils
==============
 */

 
/**
 * https://man7.org/linux/man-pages/man2/sigaction.2.html
 */
void handler(int sig, siginfo_t *info, void *ucontext) {
  ERR_PRINT("Fatal memory error occurred.\n");
  ERR_PRINT("Error is: %s\n", strsignal(info->si_signo));
  ERR_PRINT("si_code: %d\n", info->si_code);
  ERR_PRINT("si_addr: %p\n", info->si_addr);

  // Evaluate range, if a address bounds issue:
  if (info->si_code == SEGV_BNDERR) {
    ERR_PRINT("Address bounds: %p to %p\n", info->si_lower, info->si_upper);
  }
  //exit gracefully...
  abort();
}


ReturnCode replaceFileBufferInSeq(int *fd, size_t fileSize, Sequence *seq){
  //Calculate needed map size
  const size_t mask = (size_t) sysconf(_SC_PAGESIZE) - 1;
  size_t alignedCapacity = (fileSize + mask) & ~mask; // ensures we round up to page alignment 
  
  // Setup mmap: 
  void *fileMapping = mmap(NULL, alignedCapacity, PROT_READ | PROT_WRITE, MAP_PRIVATE, *fd, 0);

  if (fileMapping == MAP_FAILED) {
    ERR_PRINT("'mmap' did not succeed. ErrorNo: %s\n", strerror(errno));
    return -1;
  }

  DEBG_PRINT("Replaced file buffer with capacity:%d, size:%d, pointer %p\n",alignedCapacity,fileSize,fileMapping);
  seq->fileBuffer.capacity = alignedCapacity;
  seq->fileBuffer.data = (Atomic*) fileMapping;
  seq->fileBuffer.size = fileSize;

  return 1;
}

ReturnCode resizeFileAndMapping(int fd, void** mapping, size_t currentSize, size_t currentAlignedSize, size_t newSize, size_t newAlignedSize) {
  if (currentSize > newSize){
    // Shrink mapping first
    void* newMapping = mremap(*mapping, currentAlignedSize, newAlignedSize, 0);
    if (newMapping == MAP_FAILED){
      ERR_PRINT("MMAP resize failed.\n");
      return -1;
    } 
    // Then shrink file
    if (ftruncate(fd, newSize) < 0){
      ERR_PRINT("FD resize failed.\n");
      return -1;
    } 
    *mapping = newMapping;
    return newAlignedSize;
  } else{
    // Grow file first
    if (ftruncate(fd, newSize) < 0){
      ERR_PRINT("FD resize failed.\n");
      return -1;
    } 
    // Then grow mapping
    void* newMapping = mremap(*mapping, currentAlignedSize, newAlignedSize, MREMAP_MAYMOVE); 
    if (newMapping == MAP_FAILED){
      ERR_PRINT("MMAP resize failed.\n");
      return -1;
    } 
    
    *mapping = newMapping;
    return newAlignedSize;
  }
}

void closeAllFileResources(Sequence *seq, int fdOfCurrentOpenFile){
    munmap(fdOfCurrentOpenFile, seq->fileBuffer.capacity);
    close(fdOfCurrentOpenFile);

    if (_internalSaveAndWriteMMAP.data != NULL){
      munmap(_internalSaveAndWriteFd, _internalSaveAndWriteMMAP.capacity);
    }

    close(_internalOriginalFileCopyFd);
    close(_internalSaveAndWriteFd);
}
