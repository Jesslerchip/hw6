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
    shmid = shmget(SHMKEY, sizeof(struct SystemClock), IPC_CREAT | 0666);
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
    msqid = msgget(MSGKEY, IPC_CREAT | 0666);
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

    printf("OSS: Initializing...\n");
    
    // Init shared memory
    initSharedMemory();

    printf("OSS: Shared memory initialized\n");

    // Init system clock
    initSystemClock();

    printf("OSS: System clock initialized\n");
    
    // Init message queue
    initMessageQueue();

    printf("OSS: Message queue initialized\n");
    
    // Init page table
    initPageTable();

    printf("OSS: Page table initialized\n");
    
    // Init frame table
    initFrameTable();

    printf("OSS: Frame table initialized\n");

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

    /* VARIABLES */

    int numActiveProcesses = 0; // Number of active processes
    int numLaunchedProcesses = 0; // Number of launched processes
    int nextOutputTime = 0; // Next time to output stats to the log file and screen

    /* MAIN LOOP */   

    while (numActiveProcesses > 0 || (numLaunchedProcesses < n && numLaunchedProcesses <= 100)) {
        
        // Do a nonblocking waitpid to see if a child process has terminated. If so, free up its resources
        if (numActiveProcesses > 0) {
            // Check if a child process has terminated
            pid_t pid = waitpid(-1, NULL, WNOHANG);

            // If a child process has terminated
            if (pid > 0) {
                // Decrement number of active processes
                numActiveProcesses--;

                // Free up its resources
                // TODO
            }
        }
        
        // Determine if a new process should be launched
        if (numActiveProcesses < s && numActiveProcesses < MAX_PROCESSES) {
            // Launch a new process
            pid_t pid = fork();

            // Child process code
            if (pid == 0) { 
                execl("./worker", "worker", NULL);
                exit(0);

            // Parent process code
            } else if (pid > 0) {

                // Increment number of active processes and launched processes
                numActiveProcesses++;
                numLaunchedProcesses++;

                // Find an empty page table entry
                int pageTableEntry = -1;
                for (int i = 0; i < NUM_PAGES_PER_PROCESS * MAX_PROCESSES; i++) {
                    if (pageTable[i].pid == -1) {
                        pageTableEntry = i;
                        break;
                    }
                }

                // If no empty page table entry was found
                if (pageTableEntry == -1) {
                    perror("oss: Error: Failed to find empty page table entry");
                    exit(EXIT_FAILURE);
                }

                // Fill out page table entry
                pageTable[pageTableEntry].pid = pid;
                pageTable[pageTableEntry].frame = -1;
                pageTable[pageTableEntry].dirty = 0;
                pageTable[pageTableEntry].valid = 1;
                pageTable[pageTableEntry].referenced = 0;

                // Find an empty frame table entry
                int frameTableEntry = -1;
                for (int i = 0; i < NUM_FRAMES; i++) {
                    if (frameTable[i].occupied == 0) {
                        frameTableEntry = i;
                        break;
                    }
                }

                // If no empty frame table entry was found
                if (frameTableEntry == -1) {
                    perror("oss: Error: Failed to find empty frame table entry");
                    exit(EXIT_FAILURE);
                }

                // Fill out frame table entry
                frameTable[frameTableEntry].occupied = 1;
                frameTable[frameTableEntry].page = pageTableEntry;
                frameTable[frameTableEntry].dirty = 0;
                frameTable[frameTableEntry].valid = 1;
                frameTable[frameTableEntry].headOfQueue = 0;

                // Update page table entry with frame table entry
                pageTable[pageTableEntry].frame = frameTableEntry;


            // Error
            } else {
                perror("oss: Error: Failed to fork");
                exit(EXIT_FAILURE);
            }
        }

        // TODO: Check to see if event wait for a child is now finished and it gets granted its request. ie: Its page is swapped in.

        // TODO: Check if we have a message from a child. If so, and there is not a page fault, send a message back. If there is a pagefault, set up its waiting for an event.


        // Every half a second, output the page table, frame table, and process table to the logfile and to the screen.
        if (nextOutputTime <= getClockTime()) {
            
            // Output page table
            printf("OSS: Page table:\n");
            for (int i = 0; i < NUM_PAGES_PER_PROCESS * MAX_PROCESSES; i++) {
                // Only print if valid
                if (pageTable[i].valid == 1) {
                    printf("OSS: Page table entry %d: pid=%d, frame=%d, dirty=%d, valid=%d, referenced=%d\n", i, pageTable[i].pid, pageTable[i].frame, pageTable[i].dirty, pageTable[i].valid, pageTable[i].referenced);
                }
            }

            // Output frame table
            printf("OSS: Frame table:\n");
            for (int i = 0; i < NUM_FRAMES; i++) {
                // Only print if occupied
                if (frameTable[i].occupied == 1) {
                    printf("OSS: Frame table entry %d: occupied=%d, page=%d, dirty=%d, valid=%d, headOfQueue=%d\n", i, frameTable[i].occupied, frameTable[i].page, frameTable[i].dirty, frameTable[i].valid, frameTable[i].headOfQueue);
                }
            }

            // Output process table
            printf("OSS: Process table:\n");
            for (int i = 0; i < MAX_PROCESSES; i++) {
                printf("OSS: Process table entry %d: pid=%d, pageFault=%d, waitingForEvent=%d\n", i, i, 0, 0);
            }

            // Set next output time
            nextOutputTime += 500000000;
        }

    }


    /* END MAIN LOOP */
}