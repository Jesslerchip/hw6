// Jessica Seabolt 11/17/2023 CMP_SCI 4760 Project 6

#include <fcntl.h>
#include <time.h>

#include "header.h"

#define _GNU_SOURCE
#define MAX_PROCESSES 18

// Global variables
int shmid;
int msqid;
char logMessage[150];
struct SystemClock *sysClock;
struct PageTable *pageTable;
struct FrameTable *frameTable;
struct msgbuf inbox, outbox;

/* INIT FUNCTIONS */

// Init shared memory
void initSharedMemory() {
    // Get shared memory
    shmid = shmget(SHMKEY, sizeof(struct SystemClock), 0666);
    if (shmid == -1) {
        perror("oss: Error: Failed to get shared memory");
        exit(EXIT_FAILURE);
    }
    
    // Attach shared memory
    sysClock = (struct SystemClock *)shmat(shmid, NULL, 0);
    if (sysClock == (void *)-1) {
        perror("oss: Error: Failed to attach shared memory");
        exit(EXIT_FAILURE);
    }
}

// Init system clock
void initSystemClock() {
    // Initialize system clock
    sysClock->seconds = 0;
    sysClock->nanoseconds = 0;
}

// Init message queue
void initMessageQueue() {
    // Get message queue
    msqid = msgget(MSGKEY, 0666);
    if (msqid == -1) {
        perror("oss: Error: Failed to get message queue");
        exit(EXIT_FAILURE);
    }
}

// Init page table
void initPageTable() {
    // Allocate memory for page table
    pageTable = malloc(NUM_PAGES_PER_PROCESS * MAX_PROCESSES * sizeof(struct PageTable));
    if (pageTable == NULL) {
        perror("oss: Error: Failed to allocate memory for page table");
        exit(EXIT_FAILURE);
    }
    
    // Initialize page table
    for (int i = 0; i < NUM_PAGES_PER_PROCESS * MAX_PROCESSES; i++) {
        pageTable[i].pid = -1;
        pageTable[i].frame = -1;
        pageTable[i].dirty = 0;
        pageTable[i].valid = 0;
        pageTable[i].referenced = 0;
    }
}

// Init frame table
void initFrameTable() {
    // Allocate memory for frame table
    frameTable = malloc(NUM_FRAMES * sizeof(struct FrameTable));
    if (frameTable == NULL) {
        perror("oss: Error: Failed to allocate memory for frame table");
        exit(EXIT_FAILURE);
    }
    
    // Initialize frame table
    for (int i = 0; i < NUM_FRAMES; i++) {
        frameTable[i].occupied = 0;
        frameTable[i].page = -1;
        frameTable[i].dirty = 0;
        frameTable[i].valid = 0;
        frameTable[i].headOfQueue = 0;
    }
}

/* END INIT FUNCTIONS */

/* MISC FUNCTIONS */

// Advance simulated clock
void advanceClock(int nanoseconds) {
    sysClock->nanoseconds += nanoseconds;
    if (sysClock->nanoseconds >= 1000000000) {
        sysClock->seconds++;
        sysClock->nanoseconds -= 1000000000;
    }
}

// Get simulated clock time
int getClockTime() {
    return sysClock->seconds * 1000000000 + sysClock->nanoseconds;
}

// Get frame table entry by head of queue
int getFrameTableEntryByHeadOfQueue() {
    for (int i = 0; i < NUM_FRAMES; i++) {
        if (frameTable[i].headOfQueue == 1) {
            return i;
        }
    }
    
    perror("oss: Error: Failed to get frame table entry by head of queue");
    return -1;
}

// Function to handle signals
void handleSignal(int sig) {
	if (sig == SIGINT) {
		sprintf(logMessage, "OSS: Caught SIGINT, exiting...\n");
		printf("%s", logMessage);
	} else if (sig == SIGALRM) {
		sprintf(logMessage, "OSS: Caught SIGALRM, exiting...\n");
		printf("%s", logMessage);
	}
	shmctl(shmid, IPC_RMID, NULL);
	msgctl(msgget(ftok("oss.c", 1), 0666 | IPC_CREAT), IPC_RMID, NULL);
	exit(0);
}

void writeLog(const char* logfile, const char* message) {
	int fd = open(logfile, O_WRONLY | O_CREAT | O_APPEND, 0666);
	if (fd < 0) {
		perror("open");
		return;
	}

	dprintf(fd, "%s", message);
	close(fd);
}

/* END MISC FUNCTIONS */

/* CLEANUP FUNCTIONS */

// Cleanup shared memory
void cleanupSharedMemory() {
    // Detach shared memory
    if (shmdt(sysClock) == -1) {
        perror("oss: Error: Failed to detach shared memory");
        exit(EXIT_FAILURE);
    }
    
    // Remove shared memory
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("oss: Error: Failed to remove shared memory");
        exit(EXIT_FAILURE);
    }
}

// Cleanup message queue
void cleanupMessageQueue() {
    // Remove message queue
    if (msgctl(msqid, IPC_RMID, NULL) == -1) {
        perror("oss: Error: Failed to remove message queue");
        exit(EXIT_FAILURE);
    }
}

// Cleanup page table
void cleanupPageTable() {
    // Free page table
    free(pageTable);
}

// Cleanup frame table
void cleanupFrameTable() {
    // Free frame table
    free(frameTable);
}

/* END CLEANUP FUNCTIONS */

/* MAIN FUNCTION */

// Main
int main(int argc, char *argv[]) {
    // Seed random number generator
    srand(time(NULL));

    /* INITIALIZE */
    
    // Init shared memory
    initSharedMemory();
    
    // Init system clock
    initSystemClock();
    
    // Init message queue
    initMessageQueue();
    
    // Init page table
    initPageTable();
    
    // Init frame table
    initFrameTable();

    // Init signal handlers
	signal(SIGINT, handleSignal);
	signal(SIGALRM, handleSignal);
	alarm(5); // Set alarm for 3 seconds

    /* END INITIALIZE */

    /* ARGUMENTS */

    int n = -1; // number of processes
	int s = -1; // max simultaneous processes 
	int t = -1; // time interval
	char* logfile = NULL;

	// Parse command line arguments
	int opt;
	while ((opt = getopt(argc, argv, "hn:s:t:f:")) != -1) {
		switch(opt) {
			case 'h':
				printf("Usage: %s [-h] [-n num procs] [-s max simul procs] [-t time in nanoseconds] [-f logfile]\n", argv[0]);
				exit(0);
			case 'n':
				n = atoi(optarg);
				break;
			case 's':
				s = atoi(optarg);
				break;
			case 't':
				t = atoi(optarg);
				break;
			case 'f':
				logfile = optarg;
				break;
		}
	}

	// Handle missing arguments
	if (n == -1 || s == -1 || t == -1) {
		fprintf(stderr, "Error: Missing required arguments.\n");
		exit(1);
	} 

	// Handle invalid arguments
	if (!logfile) {
		fprintf(stderr, "Error: No logfile specified.\n");
		exit(1);
	} else if (n < 1 || s < 1 || s > 20 || t < 1) {
		fprintf(stderr, "Error: Invalid arguments.\n");
		exit(1);
	}

    /* END ARGUMENTS */

    /* MAIN LOOP */

    // TODO: Implement main loop

    /* END MAIN LOOP */
}