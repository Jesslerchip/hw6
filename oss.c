// Jessica Seabolt 11/17/2023 CMP_SCI 4760 Project 6

/* we will be implementing the FIFO (clock) page replacement algorithms. When a page-fault occurs,
it will be necessary to swap in that page. If there are no empty frames, your algorithm will select the victim frame
based on our FIFO replacement policy. This treats the frames as one large circular queue.
Each frame should also have an additional dirty bit, which is set on writing to the frame. This bit is necessary
to consider dirty bit optimization when determining how much time these operations take. The dirty bit will be
implemented as a part of the page table. 

This will be your main program and serve as the master process. You will start the operating system simulator (call
the executable oss) as one main process who will create new children. We still have as our
clock the 2 integers in shared memory.
In the beginning, oss will allocate memory for system data structures, including page table. You will also need to
create a fixed size array of structures for a page table for each process, with each process having 32k of memory
and so requiring 32 entries as the pagesize is 1k. The page table should have all the required fields that may be
implemented by bits or character data types.
Assume that your system has a total memory of 256K. You will require a frame table of 256 structures, with any
data required such as reference byte and dirty bit contained in that structure.
After the resources have been set up, allow new processes in your system just like project 5. Make sure that you
never have more than 18 user processes in the system. If you already have 18 processes, do not create any more until
some process terminates (this is probably specified by your -s parameter, but never let that get above 18). Thus, if a
user specifies an actual number of processes as 30, your hard limit will still limit it to no more than 18 processes at
any time in the system. Your user processes execute concurrently and there is no scheduling performed. They run
in a loop constantly till they have to terminate.
oss will monitor all memory references from user processes (ie: messages sent to it from user processes) and if the
reference results in a page fault, the process will be blocked till the page has been brought in. If there is no page
fault, oss just increments the clock by 100 nanoseconds (just so if the user process looks at the clock, it is different
than when it sent the request) and sends a message back. In case of page fault, oss queues the request to the device
(ie: we simulate it being blocked). Each request for disk read/write takes about 14ms to be fulfilled. In case of
page fault, the request is queued for the device and the process is suspended as no message is sent back and so the
user process is just waiting on a msgrcv (ie: it has to wait for that event to
happen). The request at the head of the queue is fulfilled once the clock has advanced by disk read/write time since
the time the request was found at the head of the queue. The fulfillment of request is indicated by showing the
page in memory in the page table. oss should periodically check if all the processes are queued for device and if so,
advance the clock to fulfill the request at the head. We need to do this to resolve any possible soft deadlock (not an
actual deadlock, we aren’t doing deadlock detection for this here!) in case memory is low and all processes end up
waiting.
While a page is referenced, oss performs other tasks on the page table as well such as updating the page reference,
setting up dirty bit, checking if the memory reference is valid (all will be valid for this project) and whether the
process has appropriate permissions on the frame (we aren’t doing frame locking, so all should be valid here), and
so on.
When a process terminates, oss should log its termination in the log file and also indicate its effective memory
access time. oss should also print its memory map every second showing the allocation of frames. You can display
unallocated frames by a period and allocated frame by a +. */

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
struct msgbuf inbox, outbox;
struct PCB processTable[MAX_PROCESSES];
struct PageTable *pageTable;
struct FrameTable *frameTable;

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

    // Init process table
    initProcessTable();
 
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

    /* VARIABLES */

    int numActiveProcesses = 0; // Number of active processes
    int numLaunchedProcesses = 0; // Number of launched processes
    int nextOutputTime = 0; // Next time to output stats to the log file and screen

    /* MAIN LOOP */   

    while (numActiveProcesses > 0 || (numLaunchedProcesses < n && numLaunchedProcesses <= 100)) {
        
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
                    for (int i = 0; i < NUM_PAGES_PER_PROCESS * MAX_PROCESSES; i++) {
                        if (pageTable[i].pid == -1) {
                            pageTableEntry = i;

                            // Update page table entry
                            pageTable[pageTableEntry].pid = pid;
                            pageTable[pageTableEntry].frame = -1;
                            pageTable[pageTableEntry].dirty = 0;
                            pageTable[pageTableEntry].valid = 1;
                            pageTable[pageTableEntry].referenced = 0;

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


                }

                // Frame table entry (256k of memory, each frame is 1k)
                for (int i = 0; i < NUM_FRAMES; i++) {
                    // Find an empty frame table entry
                    int frameTableEntry = -1;
                    for (int i = 0; i < NUM_FRAMES; i++) {
                        if (frameTable[i].occupied == 0) {
                            frameTableEntry = i;

                            // Update frame table entry
                            frameTable[frameTableEntry].occupied = 1;
                            frameTable[frameTableEntry].page = -1;
                            frameTable[frameTableEntry].dirty = 0;
                            frameTable[frameTableEntry].valid = 1;
                            frameTable[frameTableEntry].headOfQueue = 0;

                            break;
                        }
                    }

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

        // Check if a process is waiting for an event. If so, check if the event has happened. If so, send a message back to the child.
        for (int i = 0; i < MAX_PROCESSES; i++) {
            if (processTable[i].blocked == 1) {
                // Check if the event has happened
                if (processTable[i].eventWaitSec <= sysClock->seconds && processTable[i].eventWaitNano <= sysClock->nanoseconds) {
                    // Send message back to child
                    outbox.mType = processTable[i].pid;
                    outbox.mNum[0] = 0;
                    outbox.mNum[1] = 0;
                    if (msgsnd(msqid, &outbox, sizeof(outbox.mNum), 0) == -1) {
                        perror("oss: Error: Failed to send message to child");
                        exit(EXIT_FAILURE);
                    }

                    // Update PCB
                    processTable[i].blocked = 0;
                    processTable[i].eventWaitSec = 0;
                    processTable[i].eventWaitNano = 0;
                }
            }
        }

        // Check if we have a message from a child. If so, and there is not a page fault, send a message back. If there is a pagefault, set up its waiting for an event.
        if (msgrcv(msqid, &inbox, sizeof(inbox.mNum), 0, IPC_NOWAIT) != -1) {
            // Check if the message is a terminate message
            if (inbox.mNum[0] < 0) {
                // Free up its resources and log its termination
                for (int i = 0; i < MAX_PROCESSES; i++) {
                    if (processTable[i].pid == inbox.mType) {
                        // Free up its resources
                        for (int j = 0; j < NUM_PAGES_PER_PROCESS * MAX_PROCESSES; j++) {
                            if (pageTable[j].pid == inbox.mType) {
                                // Update page table entry
                                pageTable[j].pid = -1;
                                pageTable[j].frame = -1;
                                pageTable[j].dirty = 0;
                                pageTable[j].valid = 0;
                                pageTable[j].referenced = 0;
                            }
                        }
                        for (int j = 0; j < NUM_FRAMES; j++) {
                            if (frameTable[j].page == inbox.mType) {
                                // Update frame table entry
                                frameTable[j].occupied = 0;
                                frameTable[j].page = -1;
                                frameTable[j].dirty = 0;
                                frameTable[j].valid = 0;
                                frameTable[j].headOfQueue = 0;
                            }
                        }
                        for (int j = 0; j < MAX_PROCESSES; j++) {
                            if (processTable[j].pid == inbox.mType) {
                                // Update PCB
                                processTable[j].occupied = 0;
                                processTable[j].pid = -1;
                                processTable[j].eventWaitSec = 0;
                                processTable[j].eventWaitNano = 0;
                                processTable[j].blocked = 0;
                            }
                        }

                        // Log its termination
                        sprintf(logMessage, "OSS: Child %ld terminated at time %d:%d\n", inbox.mType, sysClock->seconds, sysClock->nanoseconds);
                        writeLog(logfile, logMessage);
                    }
                }

            // If the message is not a terminate message
            } else {
                // Determine if there is a page fault
                int pageFault = 0;
                for (int i = 0; i < NUM_PAGES_PER_PROCESS * MAX_PROCESSES; i++) {
                    if (pageTable[i].pid == inbox.mType && pageTable[i].valid == 1 && pageTable[i].frame == -1) {
                        pageFault = 1;
                        break;
                    }
                }

                // If there is not a page fault
                if (pageFault == 0) {
                    // Send message back to child
                    outbox.mType = inbox.mType;
                    outbox.mNum[0] = 0;
                    outbox.mNum[1] = 0;
                    if (msgsnd(msqid, &outbox, sizeof(outbox.mNum), 0) == -1) {
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
                            break;
                        }
                    }

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
                        // Find the page table entry
                        int pageTableEntry = -1;
                        for (int i = 0; i < NUM_PAGES_PER_PROCESS * MAX_PROCESSES; i++) {
                            if (pageTable[i].pid == inbox.mType && pageTable[i].valid == 1 && pageTable[i].frame == -1) {
                                pageTableEntry = i;
                                break;
                            }
                        }

                        // If no page table entry was found
                        if (pageTableEntry == -1) {
                            perror("oss: Error: Failed to find page table entry");
                            exit(EXIT_FAILURE);
                        }

                        // Update page table entry
                        pageTable[pageTableEntry].frame = emptyFrame;

                        // Update frame table entry
                        frameTable[emptyFrame].page = inbox.mType;
                        frameTable[emptyFrame].headOfQueue = 1;

                        // Send message back to child
                        outbox.mType = inbox.mType;
                        outbox.mNum[0] = 0;
                        outbox.mNum[1] = 0;
                        if (msgsnd(msqid, &outbox, sizeof(outbox.mNum), 0) == -1) {
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
                        frameTable[frameTableEntry].page = inbox.mType;
                        frameTable[frameTableEntry].headOfQueue = 1;

                        // Send message back to child
                        outbox.mType = inbox.mType;
                        outbox.mNum[0] = 0;
                        outbox.mNum[1] = 0;
                        if (msgsnd(msqid, &outbox, sizeof(outbox.mNum), 0) == -1) {
                            perror("oss: Error: Failed to send message to child");
                            exit(EXIT_FAILURE);
                        }
                    }

                // Advance simulated clock 14ms
                advanceClock(14000000);

                }
            }

        }

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