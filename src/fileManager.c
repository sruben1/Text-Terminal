#include "fileManager.h"

#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700 // fix some issues with unavailable definitions
#include <fcntl.h> // File controls
#include <sys/stat.h> // To query file statistics
#include <unistd.h> // misc. functions related t o system and I/O
#include <sys/mman.h> // mempry map support
#include <errno.h> // Error managment
#include <signal.h> // Errors & signaling
#include <sys/types.h> // Error handling & analysis tools
#include <string.h> 

#include "debugUtil.h"

  // Very helpful resources:
  //(SIGBUS) mmap c style Error handling:
  //https://gist.github.com/FauxFaux/9032a665250fbcbe6fe6060ebe93a14f
  //MMAP:
  //https://comp.anu.edu.au/courses/comp2310/labs/05-malloc/
  //https://comp.anu.edu.au/courses/comp2310/labs/07-mmio/#the-mmap-system-call
  //cpp though: https://www.sublimetext.com/blog/articles/use-mmap-with-care

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

LineBidentifier initSequenceFromOpenOrCreate(const char* pathname, Sequence* emptySequences, int *fileDescriptorToReturn, LineBidentifier lbStdForNewFile){
  if ( (*fileDescriptorToReturn = open(pathname, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR /*Read&Write permissions*/)) < 0){
    ERR_PRINT("Failed to open file!\n");
    fileDescriptorToReturn = NULL;
    return -1;
  }

  // get size:
  struct stat buffer;
  if(fstat(fileDescriptorToReturn, &buffer) < 0){
    ERR_PRINT("Failed to read stats form file: %s\n", strerror(errno));
    return -1;
  }
  size_t fileSize = (size_t) buffer.st_size;

  //Calculate needed map size
  const size_t mask = (size_t) sysconf(_SC_PAGESIZE) - 1;
  size_t alignedCapacity = (fileSize + mask) & ~mask; // ensures we round up to page alignment 
  
  // Setup mmap: 
  Atomic *fileMapping = (Atomic) mmap(NULL, alignedCapacity, PROT_READ, MAP_PRIVATE, fileDescriptorToReturn, 0);

  if (fileMapping == MAP_FAILED) {
    ERR_PRINT("'mmap' did not succeed. ErrorNo: %s\n", strerror(errno));
    return 1;
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

  //TODO : query line break standard
  LineBstd analyzedLineBstd = NO_INIT; // Replace me

  /*Sequence* newSeq = empty();
  newSeq->fileBuffer.capacity = alignedCapacity;
  newSeq->fileBuffer.data = fileMapping;
  newSeq->fileBuffer.size = fileSize;*/

}