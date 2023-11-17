#ifndef HEADER_H
#define HEADER_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/ipc.h>

#define SHMKEY 0x1234
#define MSGKEY 0x2468
#define PAGE_SIZE 1024
#define NUM_PAGES_PER_PROCESS 32
#define NUM_FRAMES 256

// SystemClock struct
struct SystemClock {
	unsigned int seconds; // Simulated seconds
	unsigned int nanoseconds; // Simulated nanoseconds
};

// Message queue struct
struct msgbuf {
	long mType; // Message type
	int mNum; // Message content
};

// Page table struct
struct PageTable {
    int pid; // Process ID
    int frame; // Frame number
    int dirty; // Dirty
    int valid; // Valid
    int referenced; // Referenced
};

// Frame table struct
struct FrameTable {
    int occupied; // Occupied
    int page; // Page number
    int dirty; // Dirty
    int valid; // Valid
    int headOfQueue; // Head of queue
};

// Function prototypes
void initSharedMemory();
void initSystemClock();
void initMessageQueue();
void initPageTable();
void initFrameTable();


#endif
