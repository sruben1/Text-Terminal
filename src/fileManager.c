#include "fileManager.h"

#define _GNU_SOURCE
#include <fcntl.h> // File controls
#include <sys/stat.h> // To query file statistics
#include <unistd.h> // misc. functions related t o system and I/O
#include <sys/mman.h> // mempry map support
#include <sys/sendfile.h> // simpler file copy
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
int _mainFileFd = -1; // Initalized at open
Buffer _mainFileSaveAndWriteMMAP = {NULL, 0, 0}; // If file has some content: initially put into write buffer otherwise nothing there 
int _internalOriginalFileCopyFd = -1; // If file has some content before opening it with TxT: copy of original state upon first save and sequence then redirected to mmap on this copy. 



// Forward declarations
/*---- Utilities ----*/
ReturnCode writeSequenceToMapping(Atomic* writeMapping, size_t newSize, size_t newAlignedSize, Sequence* seq);
ReturnCode resizeFileAndMapping(int fd, void** mapping, size_t currentSize, size_t currentAlignedSize, size_t newSize, size_t newAlignedSize);
void handler(int sig, siginfo_t *info, void *ucontext);
ReturnCode replaceFileBufferInSeq(int fd, size_t fileSize, Sequence *seq);
size_t simpleFileCopy(int sourceFd, int destFd,  size_t fileSize);


LineBstd initSequenceFromOpenOrCreate(const char* pathname, Sequence* emptySequences, LineBstd lbStdForNewFile){
  if(_mainFileFd >= 0){
    ERR_PRINT("Error, Close currently open file first!\n");
    fprintf(stderr, "Error, Close currently open file first!\n");
    return NO_INIT;
  }
  if ( (_mainFileFd = open(pathname, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR /*user has Read&Write permissions*/)) < 0){ // O_RDWR flag for R&W open; O_RDONLY flag for read only
    ERR_PRINT("Failed to open file: %s!\n", strerror(errno));
    _mainFileFd = -1;
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
  if(fstat(_mainFileFd, &buffer) < 0){
    ERR_PRINT("Failed to read stats form file: %s\n", strerror(errno));
    return -1;
  }

  if (buffer.st_size > 0){
    if(replaceFileBufferInSeq(_mainFileFd, buffer.st_size, emptySequences) < 0){
      ERR_PRINT("Failed to init MMAP into sequence structure.\n");
      return -1;
    }
    DEBG_PRINT("MMAP done moving to next steps.\n");

    LineBstd foundStd = findMostLikelyLineBreakStd(emptySequences);
    if(foundStd == NO_INIT){
      ERR_PRINT("failed to find lineBstd for file...\n");
      if (lbStdForNewFile == NO_INIT){
        ERR_PRINT("Aborting since no std specified\n");
        fprintf(stderr, "Please specify a line break std in arguments for this file (could not identify it automatically).\n");
      }
    } else{
      lbStdForNewFile = foundStd;
    }

  } else{
    /*nothing to do since empty() already took care of null assignments to file buffer*/
  }

  return lbStdForNewFile;
}

/**
 * Save sequence to file efficiently using memory mappings and backup to temporary files in between
 */
ReturnCode saveSequenceToOpenFile(Sequence* sequence) {
  if ( _mainFileFd == NULL || _mainFileFd < 0  || sequence == NULL ) {
    ERR_PRINT("Invalid parameters to saveSequenceToFile, mainFd value:%d, seq ptr:%p\n", _mainFileFd, sequence);
    return -1;
  }

  // Calculate required size (size for current sequence state)
  size_t requiredSize = (size_t) getCurrentTotalSize(sequence);
  DEBG_PRINT("Got SizeToSave:%d\n", requiredSize);
  if (requiredSize < 0) {
    ERR_PRINT("calculation failed\n");
    return -1;
  }
  /* CASE of EMPTY but initial open not empty*/

  struct stat mainFileStat;
  if (fstat(_mainFileFd, &mainFileStat) < 0) {
    ERR_PRINT("Failed to get file stats: %s\n", strerror(errno));
    return -1;
  }

  bool skipBackup = false;


  // >> Perform file Copy of file (of opened file, if it was not empty) in it's original state to keep sequence and piece table consistent
  if (_internalOriginalFileCopyFd == -1 && sequence->fileBuffer.capacity != 0){
    DEBG_PRINT("Started needed orig copy...\n");
    char backupPath[] = "/tmp/TxTinternal-OrigState-XXXXXX";
    _internalOriginalFileCopyFd = mkstemp(backupPath);
    if (_internalOriginalFileCopyFd < 0) {
      ERR_PRINT("Failed to create copy of original file, aborting save: %s\n", strerror(errno));
      return -1;
    }

    off_t offset = 0;
    size_t copied = simpleFileCopy(_mainFileFd, _internalOriginalFileCopyFd, mainFileStat.st_size);
    // copy_file_range(_mainFileFd, &offset, _internalOriginalFileCopyFd, NULL, mainFileStat.st_size, 0);
    // if (copied < 0 && errno == EXDEV){ // WSL fallback
    //   ERR_PRINT("Failed to use special linux copy: %s ; using fall back method instead.\n", strerror(errno));
    //   copied = 
    // }
  
    if (copied != mainFileStat.st_size) {
      ERR_PRINT("Failed to create complete copy of original (copied %ld != %ld), aborting backup and save: %s\n", (long int) copied, (long int) mainFileStat.st_size, strerror(errno));
      return -1;
    }

    //Swich out internal and new copy:
    _mainFileSaveAndWriteMMAP.capacity = sequence->fileBuffer.capacity;
    _mainFileSaveAndWriteMMAP.data = sequence->fileBuffer.data;
    _mainFileSaveAndWriteMMAP.size = sequence->fileBuffer.size;
    //now replace internal mapping to map to original background copy
    if(replaceFileBufferInSeq(_internalOriginalFileCopyFd, (size_t) copied, sequence) < 0){
      ERR_PRINT("Failed to replace MMAP into sequence structure.\n");
      return -1;
    }
    skipBackup = true; // since copy here essentially is already identical to first backup

    DEBG_PRINT("Ended needed orig copy...\n");
  }

  int backupFd = -1;


  // >> If useful, perform file backup before save
  if (mainFileStat.st_size > 0 && !skipBackup) {
    // Create temporary backup using copy_file_range (Linux-specific, efficient)
    char backupPath[] = "/tmp/TxTinternal-filebackup-XXXXXX";
    backupFd = mkstemp(backupPath);
    if (backupFd < 0) {
      ERR_PRINT("Failed to create backup file, aborting save: %s\n", strerror(errno));
      return -1;
    }
    
    DEBG_PRINT("Starting needed temp copy of actual file...\n");
    // Copy save file to backup
    off_t offset = 0;
    size_t copied = simpleFileCopy(_mainFileFd, backupFd, mainFileStat.st_size);
    // copy_file_range(_mainFileFd, &offset, backupFd, NULL, mainFileStat.st_size, 0);
    // if (copied < 0 && errno == EXDEV){ // WSL fallback
    //   ERR_PRINT("Failed to use special linux copy: %s ; using fall back method instead.\n", strerror(errno));
    //   copied = 
    // }
    if (copied != mainFileStat.st_size) {
      ERR_PRINT("Failed to create complete backup (copied %d), aborting backup and save: %s\n", copied, strerror(errno));
      return -1;
    }
    DEBG_PRINT("Ended needed temp copy...\n");
  }


  // >> Prepare actual save operation
  const size_t mask = (size_t) sysconf(_SC_PAGESIZE) - 1;
  size_t newAlignedSize = (requiredSize + mask) & ~mask; 
  DEBG_PRINT("Got aligned size:%d\n", newAlignedSize);

  if(_mainFileSaveAndWriteMMAP.data != NULL && _mainFileSaveAndWriteMMAP.size == requiredSize && _mainFileSaveAndWriteMMAP.capacity == newAlignedSize){
    //Nothing to do
  } else {
    resizeFileAndMapping(_mainFileFd, (void**) &_mainFileSaveAndWriteMMAP.data, sequence->fileBuffer.size, sequence->fileBuffer.capacity, requiredSize, newAlignedSize);
    _mainFileSaveAndWriteMMAP.size = requiredSize;
    _mainFileSaveAndWriteMMAP.capacity = newAlignedSize;
  }
  if (_mainFileSaveAndWriteMMAP.data == MAP_FAILED) {
    ERR_PRINT("Failed to create updated write mapping, saving sequence to other backup: %s\n", strerror(errno));
    if (backupFd >= 0) {
      close(backupFd);
    }
    return -1;
  }
  

  // >> Write sequence data to mapped memory & ensure sync
  if (writeSequenceToMapping(_mainFileSaveAndWriteMMAP.data, requiredSize, newAlignedSize, sequence) > 0) {
    // Ensure data is written to disk
    if (msync(_mainFileSaveAndWriteMMAP.data, newAlignedSize, MS_SYNC) < 0) {
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


ReturnCode replaceFileBufferInSeq(int fd, size_t fileSize, Sequence *seq){
  if (fd < 0 || fileSize <= 0 || seq == NULL) {
    ERR_PRINT("Invalid parameters to replaceFileBufferInSeq: fd:%d, size:%d, seqPtr%d\n",fd, fileSize, seq);
    return -1;
  }

  //Calculate needed map size
  const size_t mask = (size_t) sysconf(_SC_PAGESIZE) - 1;
  size_t alignedCapacity = (fileSize + mask) & ~mask; // ensures we round up to page alignment 
  
  // Setup mmap: 
  void *fileMapping = mmap(NULL, alignedCapacity, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

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

/**
 * WSL compatible (fallback) file copy.
 */
size_t simpleFileCopy(int sourceFd, int destFd, size_t fileSize) {
  // Ensure files ready for operation
  if (lseek(sourceFd, 0, SEEK_SET) < 0 || lseek(destFd, 0, SEEK_SET) < 0) {
    return -1;
  }
  
  size_t totalCopied = 0;
  const size_t MAX_CHUNK_SIZE = 1024 * 1024 * 1024; // 64MB chunks
  
  // Perform actual copy operations
  while (totalCopied < fileSize) {
    size_t remainingBytes = fileSize - totalCopied;
    size_t bytesToCopy = (remainingBytes > MAX_CHUNK_SIZE) ? MAX_CHUNK_SIZE : remainingBytes;
    
    off_t offset = totalCopied;
    ssize_t copied = sendfile(destFd, sourceFd, &offset, bytesToCopy);
    
    if (copied < 0) {
      // If sendfile failed
      ERR_PRINT("sendfile failed, error: %s\n", strerror(errno));
    }
    
    if (copied == 0) {
      //EOF Issue
      ERR_PRINT("Unexpected EOF during copy at %d bytes\n", totalCopied);
      return -1;
    }

    DEBG_PRINT("file copy copied chunk total now: %ld\n", totalCopied);
    totalCopied += copied;
    
  }
  
  return totalCopied;

}

/**
 * Ensures file mmaps are correctly initalized and sized if we whish to write new (different) data into the files. 
 */
ReturnCode resizeFileAndMapping(int fd, void** mapping, size_t currentSize, size_t currentAlignedSize, size_t newSize, size_t newAlignedSize) {
  if (mapping == NULL || fd < 0 || newAlignedSize == 0) {
    ERR_PRINT("Invalid mapping pointer\n");
    return -1;
  }

  if(*mapping == NULL || *mapping == MAP_FAILED){

    if (ftruncate(fd, newSize) < 0){
        ERR_PRINT("FD initial resize failed: %s\n", strerror(errno));
        return -1;
    }
    
    *mapping = mmap(NULL, newAlignedSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    
    if (*mapping == MAP_FAILED) {
      ERR_PRINT("MMAP initial assignment (in resize) failed: %s\n", strerror(errno));
      *mapping = NULL;
      return -1;
    }
    
    DEBG_PRINT("Created mapping: size:%d, aligned:%d, ptr:%p\n", newSize, newAlignedSize, *mapping);
    return 1;
  }

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
    return 1;
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
    return 1;
  }
}

#define MAX_COPY_CHUNK_SIZE (250 * 1024 * 1024)

/**
 * Write sequence content to the mmap memory.
 */
ReturnCode writeSequenceToMapping(Atomic* writeMapping, size_t newSize, size_t newAlignedSize, Sequence* seq) {

  if (writeMapping == NULL || writeMapping == MAP_FAILED || seq == NULL) {
    ERR_PRINT("Invalid parameters passed to write sequence mapPtr:%p, failed:%d, seqPtr:%p\n",writeMapping, writeMapping == MAP_FAILED, seq);
    return -1;
  }

  DEBG_PRINT("Testing write access to mapping at %p\n", writeMapping);
  volatile Atomic testByte = 0x42;
  writeMapping[0] = testByte;
  if (writeMapping[0] != testByte) {
    ERR_PRINT("Write mapping is not writable!\n");
    return -1;
  }

 // Use size_t for all size-related variables to handle large files
  size_t size = 0;
  size_t blockOffset = 0;
  size_t atomicsCount = 0;
  size_t rollingAtomicCount = 0;
  size_t writeOffset = 0; 
  Atomic *currentItemBlock = NULL;

  while (atomicsCount < newSize) {
    if (rollingAtomicCount >= size) {
      blockOffset = blockOffset + rollingAtomicCount;
      atomicsCount += rollingAtomicCount; 
      rollingAtomicCount = 0;
      DEBG_PRINT("Requesting next block in seek, at atomic:%d\n", blockOffset);
      size = getItemBlock(seq, blockOffset, &currentItemBlock);
      DEBG_PRINT("New blockOffset=%d, size=%d\n", blockOffset, size);
      
      if (size <= 0 || currentItemBlock[0] == END_OF_TEXT_CHAR) {
        ERR_PRINT("Position at end of file or size<0:%d curr block start:%d\n", size, blockOffset);
        return -1;
      }
    }

    // Safety check parameters for copy operation
    // Safety check parameters for copy operation - use size_t throughout
    size_t atomicsRemaining = newSize - atomicsCount;
    size_t atomicsInThisBlock = size - rollingAtomicCount;
    size_t atomicsToCopy = (atomicsRemaining < atomicsInThisBlock) ? atomicsRemaining : atomicsInThisBlock;
    
    
    if (atomicsToCopy > MAX_COPY_CHUNK_SIZE) {
      atomicsToCopy = MAX_COPY_CHUNK_SIZE;
    }

    // Bounds check 
    if (writeOffset + atomicsToCopy > newAlignedSize) {
      ERR_PRINT("Write would exceed buffer bounds\n");
      return -1;
    }
    
    // Bounds check 
    if (writeOffset + atomicsToCopy > newAlignedSize) {
      ERR_PRINT("Write would exceed buffer bounds\n");
      return -1;
    }

    // Copy data to write buffer
    DEBG_PRINT("Writing %d atomics to offset %zu\n", atomicsToCopy, writeOffset);
    memcpy(writeMapping + writeOffset, currentItemBlock + rollingAtomicCount, atomicsToCopy * sizeof(Atomic));
    
    // Update counters
    rollingAtomicCount += atomicsToCopy;
    writeOffset += atomicsToCopy;
    
    // Exit early if still some inconsistency...
    if (atomicsCount + rollingAtomicCount >= newSize) {
      DEBG_PRINT("Completed writing all %zu atomics\n", newSize);
      break;
    }
  }

  // After your write loop completes:
if (writeOffset < newAlignedSize) {
    writeMapping[writeOffset] = END_OF_TEXT_CHAR;
}

  return 1;
}

void closeAllFileResources(Sequence *seq){
  // Unmap temp copy
  if(_internalOriginalFileCopyFd >= 0){
    munmap(seq->fileBuffer.data, seq->fileBuffer.capacity);
  }

  // Unmap read/write
  if (_mainFileSaveAndWriteMMAP.data != NULL) {
    munmap(_mainFileSaveAndWriteMMAP.data, _mainFileSaveAndWriteMMAP.capacity);
    _mainFileSaveAndWriteMMAP.data = NULL;
    _mainFileSaveAndWriteMMAP.size = 0;
    _mainFileSaveAndWriteMMAP.capacity = 0;
  }

    close(_internalOriginalFileCopyFd);
    _internalOriginalFileCopyFd = -1;
    close(_mainFileFd);
    _mainFileFd  = -1;
}
