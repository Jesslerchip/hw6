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
	int mNum[2]; // Message content
};

struct PCB {
    int occupied; // either true or false
    pid_t pid; // process id of this child
    int eventWaitSec; // when does its event happen?
    int eventWaitNano; // when does its event happen?
    int blocked; // is this process waiting on event?
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
