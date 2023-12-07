// Jessica Seabolt 11/17/2023 CMP_SCI 4760 Project 6

#include <fcntl.h>
#include <time.h>
#include <string.h>

#include "header.h"

#define _GNU_SOURCE
#define MAX_PROCESSES 18

// Global variables
int shmid;
int msqid;
char logMessage[150];
struct SystemClock *sysClock;
struct msgbuf inbox, outbox;
struct PCB processTable[MAX_PROCESSES];
struct PageTable *pageTable;
struct FrameTable *frameTable;
struct timespec lastOutputTime, currentTime;

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

// Init process table
void initProcessTable() {
    // Initialize process table
    for (int i = 0; i < MAX_PROCESSES; i++) {
        processTable[i].occupied = 0;
        processTable[i].pid = -1;
        processTable[i].eventWaitSec = 0;
        processTable[i].eventWaitNano = 0;
        processTable[i].neededPage = -1;
        processTable[i].blocked = 0;
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
        pageTable[i].valid = 1;
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
        frameTable[i].valid = 1;
        frameTable[i].headOfQueue = 0;
    }

    // Set head of queue to first frame
    frameTable[0].headOfQueue = 1;
}

void initLogFile(char* logfile) {
    // Open logfile
    int fd = open(logfile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        perror("open");
        return;
    }

    // Wipe logfile for use
    dprintf(fd, "Logfile for oss.c\n");

    // Close logfile
    close(fd);
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
    printf("%s", message);
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

    initSharedMemory();
    initSystemClock();
    initMessageQueue();
    initProcessTable();
    initPageTable();
    initFrameTable();

    // Set last output time
    clock_gettime(CLOCK_MONOTONIC, &lastOutputTime);
    unsigned int nextOutputTime = (lastOutputTime.tv_sec * 1000000000 + lastOutputTime.tv_nsec);

    // Init signal handlers
	signal(SIGINT, handleSignal);
	signal(SIGALRM, handleSignal);
	alarm(5); // Set alarm for 5 seconds

    /* END INITIALIZE */

    /* ARGUMENTS */

    int n = -1; // number of processes
	int s = -1; // max simultaneous processes 
	char* logfile = NULL;

	// Parse command line arguments
	int opt;
	while ((opt = getopt(argc, argv, "hn:s:t:f:")) != -1) {
		switch(opt) {
			case 'h':
				printf("Usage: %s [-h] [-n num procs] [-s max simul procs] [-f logfile]\n", argv[0]);
				exit(0);
			case 'n':
				n = atoi(optarg);
				break;
			case 's':
				s = atoi(optarg);
				break;
			case 'f':
				logfile = optarg;
				break;
		}
	}

	// Handle missing arguments
	if (n == -1 || s == -1) {
		fprintf(stderr, "Error: Missing required arguments.\n");
		exit(1);
	} 

	// Handle invalid arguments
	if (!logfile) {
		fprintf(stderr, "Error: No logfile specified.\n");
		exit(1);
	} else if (n < 1 || s < 1 || s > 20) {
		fprintf(stderr, "Error: Invalid arguments.\n");
		exit(1);
	}

    /* END ARGUMENTS */

    /* INIT LOGFILE */

    initLogFile(logfile);

    /* END INIT LOGFILE */

    /* VARIABLES */

    int numActiveProcesses = 0; // Number of active processes
    int numLaunchedProcesses = 0; // Number of launched processes

    /* MAIN LOOP */   

    while (numActiveProcesses > 0 || (numLaunchedProcesses < n && numLaunchedProcesses <= 100)) {

        // Check if any processes have terminated
        int status;
        int termPid = waitpid(-1, &status, WNOHANG);
        if (termPid > 0 ) {
            // Free up its resources and log its termination
            for (int i = 0; i < MAX_PROCESSES; i++) {
                if (processTable[i].pid == termPid) {
                    // Free up its resources
                    for (int j = 0; j < NUM_PAGES_PER_PROCESS * MAX_PROCESSES; j++) {
                        if (pageTable[j].pid == termPid) {
                            // Update page table entry and frame table entry
                            frameTable[pageTable[j].frame].occupied = 0;
                            frameTable[pageTable[j].frame].page = -1;
                            frameTable[pageTable[j].frame].dirty = 0;
                            frameTable[pageTable[j].frame].valid = 1;
                            pageTable[j].pid = -1;
                            pageTable[j].frame = -1;
                            pageTable[j].dirty = 0;
                            pageTable[j].valid = 1;
                            pageTable[j].referenced = 0;
                        }
                    }
                    for (int j = 0; j < MAX_PROCESSES; j++) {
                        if (processTable[j].pid == termPid) {
                            // Update PCB
                            processTable[j].occupied = 0;
                            processTable[j].pid = -1;
                            processTable[j].eventWaitSec = 0;
                            processTable[j].eventWaitNano = 0;
                            processTable[j].blocked = 0;
                        }
                    }

                    // Log its termination
                    sprintf(logMessage, "OSS: Child %d terminated at time %d:%d\n", inbox.mData.pid, sysClock->seconds, sysClock->nanoseconds);
                    writeLog(logfile, logMessage);
                }
            }
        }

        // Determine if a new process should be launched
        if (numActiveProcesses < s && numActiveProcesses < MAX_PROCESSES && numLaunchedProcesses < n && numLaunchedProcesses <= 100) {
            // Launch a new process
            pid_t pid = fork();

            // Child process code
            if (pid == 0) { 
                execl("./user_proc", "user_proc", NULL);
                exit(0);

            // Parent process code
            } else if (pid > 0) {

                // Increment number of active processes and launched processes
                numActiveProcesses++;
                numLaunchedProcesses++;

                // Assign the process its pages (32k of memory, each page is 1k)
                for (int i = 0; i < NUM_PAGES_PER_PROCESS; i++) {
                    // Find an empty page table entry
                    int pageTableEntry = -1;
                    for (int j = 0; j < NUM_PAGES_PER_PROCESS * MAX_PROCESSES; j++) {
                        if (pageTable[j].pid == -1) {
                            pageTableEntry = j;

                            // Update page table entry
                            pageTable[pageTableEntry].pid = pid;
                            pageTable[pageTableEntry].frame = -1;
                            pageTable[pageTableEntry].dirty = 0;
                            pageTable[pageTableEntry].valid = 1;
                            pageTable[pageTableEntry].referenced = 1;

                            break;
                        }
                    }

                    // If no empty page table entry was found
                    if (pageTableEntry == -1) {
                        perror("oss: Error: Failed to find empty page table entry");
                        // Output page table
                        printf("OSS: Page table:\n");
                        for (int i = 0; i < NUM_PAGES_PER_PROCESS * MAX_PROCESSES; i++) {
                            printf("OSS: Page table entry %d: pid=%d, frame=%d, dirty=%d, valid=%d, referenced=%d\n", i, pageTable[i].pid, pageTable[i].frame, pageTable[i].dirty, pageTable[i].valid, pageTable[i].referenced);
                        }
                        exit(EXIT_FAILURE);
                    }

                    // Find an empty frame table entry
                    int frameTableEntry = getFrameTableEntryByHeadOfQueue();

                    // Update frame table entry
                    frameTable[frameTableEntry].occupied = 1;
                    frameTable[frameTableEntry].page = -1;
                    frameTable[frameTableEntry].dirty = 0;
                    frameTable[frameTableEntry].valid = 1;
                    frameTable[frameTableEntry].headOfQueue = 0;

                    // Move head of queue to the next frame
                    frameTable[(frameTableEntry + 1) % NUM_FRAMES].headOfQueue = 1;

                    // If no empty frame table entry was found
                    if (frameTableEntry == -1) {
                        perror("oss: Error: Failed to find empty frame table entry");
                        // Output frame table
                        printf("OSS: Frame table:\n");
                        for (int i = 0; i < NUM_FRAMES; i++) {
                            printf("OSS: Frame table entry %d: occupied=%d, page=%d, dirty=%d, valid=%d, headOfQueue=%d\n", i, frameTable[i].occupied, frameTable[i].page, frameTable[i].dirty, frameTable[i].valid, frameTable[i].headOfQueue);
                        }
                        exit(EXIT_FAILURE);
                    }

                    // Update page table entry and frame table entry to point to each other
                    pageTable[pageTableEntry].frame = frameTableEntry;
                    frameTable[frameTableEntry].page = pageTableEntry;

                    // Simulate time spent swapping in the page
                    advanceClock(14000000);


                }


                // Add process to PCB
                for (int i = 0; i < MAX_PROCESSES; i++) {
                    if (processTable[i].occupied == 0) {
                        processTable[i].occupied = 1;
                        processTable[i].pid = pid;
                        processTable[i].eventWaitSec = 0;
                        processTable[i].eventWaitNano = 0;
                        processTable[i].blocked = 0;
                        break;
                    }
                    // If no empty PCB entry was found
                    else if (i == MAX_PROCESSES - 1) {
                        perror("oss: Error: Failed to find empty PCB entry");
                        exit(EXIT_FAILURE);
                    }
                }

            // Error
            } else {
                perror("oss: Error: Failed to fork");
                exit(EXIT_FAILURE);
            }
        }

        // Check if a process is waiting for an event. If so, check if the event has happened. If so, swap in the page and send a message back to the child.
        for (int i = 0; i < MAX_PROCESSES; i++) {
            if (processTable[i].blocked == 1) {
                // Check if the event has happened
                if (processTable[i].eventWaitSec * 1000000000 + processTable[i].eventWaitNano + 30000000 <= getClockTime()) {

                    int page = processTable[i].neededPage;

                    // Determine if there is an empty frame
                    int emptyFrame = -1;
                    for (int i = 0; i < NUM_FRAMES; i++) {
                        if (frameTable[i].occupied == 0) {
                            emptyFrame = i;
                            break;
                        }
                    }

                    // If there is an empty frame
                    if (emptyFrame != -1) {

                        // Update page table entry
                        pageTable[page].frame = emptyFrame;

                        // Update frame table entry
                        frameTable[emptyFrame].page = page;
                        frameTable[emptyFrame].headOfQueue = 1;

                        // Send message back to child
                        outbox.mType = inbox.mData.pid;
                        if (msgsnd(msqid, &outbox, sizeof(outbox.mData), 0) == -1) {
                            perror("oss: Error: Failed to send message to child");
                            exit(EXIT_FAILURE);
                        }

                    // If there is not an empty frame
                    } else {
                        // Find the frame table entry
                        int frameTableEntry = getFrameTableEntryByHeadOfQueue();

                        // If no frame table entry was found
                        if (frameTableEntry == -1) {
                            perror("oss: Error: Failed to find frame table entry");
                            exit(EXIT_FAILURE);
                        }

                        // Update page table entry
                        pageTable[frameTable[frameTableEntry].page].frame = -1;

                        // Update frame table entry
                        frameTable[frameTableEntry].page = page;
                        frameTable[frameTableEntry].headOfQueue = 1;

                        // Send message back to child
                        outbox.mType = inbox.mData.pid;
                        if (msgsnd(msqid, &outbox, sizeof(outbox.mData), 0) == -1) {
                            perror("oss: Error: Failed to send message to child");
                            exit(EXIT_FAILURE);
                        }
                    }


                    // Send message back to child
                    outbox.mType = processTable[i].pid;
                    if (msgsnd(msqid, &outbox, sizeof(outbox.mData), 0) == -1) {
                        perror("oss: Error: Failed to send message to child");
                        exit(EXIT_FAILURE);
                    }

                    // Update PCB
                    processTable[i].blocked = 0;
                    processTable[i].eventWaitSec = 0;
                    processTable[i].eventWaitNano = 0;
                    processTable[i].neededPage = -1;
                }
            }
        }

        // Check if we have a message from a child. If so, and there is not a page fault, send a message back. If there is a pagefault, set up its waiting for an event.
        if (msgrcv(msqid, &inbox, sizeof(inbox.mData), 1, IPC_NOWAIT) != -1) {

            // Extract the page from the message
            int page = inbox.mData.address / 1024;

            // Determine if there is a page fault by checking if the requested page is in a frame
            int pageFault = 0;
            if (pageTable[page].valid == 0 || pageTable[page].frame == -1) {
                pageFault = 1;
            }


            // If there is not a page fault
            if (pageFault == 0) {
                // Check if the message is a read or write
                if (inbox.mData.readWrite == 1) {
                    // Set dirty bit
                    for (int i = 0; i < NUM_PAGES_PER_PROCESS * MAX_PROCESSES; i++) {
                        if (pageTable[i].pid == inbox.mData.pid && pageTable[i].valid == 1 && pageTable[i].frame != -1) {
                            pageTable[i].dirty = 1;

                            // Add 20ms to simulated clock to simulate write time
                            advanceClock(20000000);

                            break;
                        }
                    }
                }
                else {
                    // Add 5ms to simulated clock to simulate read time
                    advanceClock(10000000);
                }

                // Update page table entry
                pageTable[page].referenced = 1;


                // Send message back to child
                outbox.mType = inbox.mData.pid;
                if (msgsnd(msqid, &outbox, sizeof(outbox.mData), 0) == -1) {
                    perror("oss: Error: Failed to send message to child");
                    exit(EXIT_FAILURE);
                }

            // If there is a page fault, swap in the page
            } else {
                // Set up its waiting for an event
                for (int i = 0; i < MAX_PROCESSES; i++) {
                    if (processTable[i].pid == inbox.mType) {
                        processTable[i].blocked = 1;
                        processTable[i].eventWaitSec = sysClock->seconds;
                        processTable[i].eventWaitNano = sysClock->nanoseconds + 14000000;
                        processTable[i].neededPage = page;
                        break;
                    }
                }

            // Advance simulated clock 14ms
            advanceClock(10000000);

            }
        }

        clock_gettime(CLOCK_MONOTONIC, &currentTime);
        unsigned int elapsedTime = (currentTime.tv_sec - lastOutputTime.tv_sec) * 1000000000 + (currentTime.tv_nsec - lastOutputTime.tv_nsec);


        // Every half a second, output the page table, frame table, and process table to the logfile and to the screen
        if (elapsedTime >= nextOutputTime) {

            // Output simulated clock
            sprintf(logMessage, "OSS: Simulated clock: %d:%d\n", sysClock->seconds, sysClock->nanoseconds);
            writeLog(logfile, logMessage);

            // Output page table
            char *pageTableString = malloc(NUM_PAGES_PER_PROCESS * MAX_PROCESSES * 256 * sizeof(char));

            if (pageTableString == NULL) {
                perror("oss: Error: Failed to allocate memory for page table string");
                exit(EXIT_FAILURE);
            }

            strcpy(pageTableString, "Page table:\n");

            for (int i = 0; i < NUM_PAGES_PER_PROCESS * MAX_PROCESSES; i++) {
                if (pageTable[i].pid != -1) {
                    char pageTableEntryString[256];
                    sprintf(pageTableEntryString, "OSS: Page table entry %d: pid=%d, frame=%d, dirty=%d, valid=%d, referenced=%d\n", i, pageTable[i].pid, pageTable[i].frame, pageTable[i].dirty, pageTable[i].valid, pageTable[i].referenced);
                    strcat(pageTableString, pageTableEntryString);
                }   
            }

            writeLog(logfile, pageTableString);
            free(pageTableString);

            // Output frame table
            char *frameTableString = malloc(NUM_FRAMES * 256 * sizeof(char));
            if (frameTableString == NULL) {
                perror("oss: Error: Failed to allocate memory for frame table string");
                exit(EXIT_FAILURE);
            }

            strcpy(frameTableString, "Frame table:\n");

            for (int i = 0; i < NUM_FRAMES; i++) {
                if (frameTable[i].occupied == 1) {
                    char frameTableEntryString[256];
                    sprintf(frameTableEntryString, "OSS: Frame table entry %d: occupied=%d, page=%d, dirty=%d, valid=%d, headOfQueue=%d\n", i, frameTable[i].occupied, frameTable[i].page, frameTable[i].dirty, frameTable[i].valid, frameTable[i].headOfQueue);
                    strcat(frameTableString, frameTableEntryString);
                }   
            }

            writeLog(logfile, frameTableString);
            free(frameTableString);

            // Output process table
            char *processTableString = malloc(MAX_PROCESSES * 256 * sizeof(char));
            if (processTableString == NULL) {
                perror("oss: Error: Failed to allocate memory for process table string");
                exit(EXIT_FAILURE);
            }

            strcpy(processTableString, "Process table:\n");

            for (int i = 0; i < MAX_PROCESSES; i++) {
                if (processTable[i].occupied == 1) {
                    char processTableEntryString[256];
                    sprintf(processTableEntryString, "OSS: Process table entry %d: pid=%d, eventWaitSec=%d, eventWaitNano=%d, neededPage=%d, blocked=%d\n", i, processTable[i].pid, processTable[i].eventWaitSec, processTable[i].eventWaitNano, processTable[i].neededPage, processTable[i].blocked);
                    strcat(processTableString, processTableEntryString);
                }   
            }

            writeLog(logfile, processTableString);
            free(processTableString);

            // Set next output time
            nextOutputTime += 500000000;
        }

        // Advance simulated clock 10ms
        advanceClock(10000000);

    }


    /* END MAIN LOOP */
}