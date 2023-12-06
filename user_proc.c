/* The user processes will go in a loop, sending messages to oss indicating they want to make a memory request.
Each user process generates memory references to one of its locations. When a process needs to generate an address
to request, it simply generates a random value from 0 to the limit of the pages that process would have access to
(32).
Now you have the page of the request, but you need the offset still. Multiply that page number by 1024 and then
add a random offset of from 0 to 1023 to get the actual memory address requested. Note that we are only simulating
this and actually do not have anything to read or write.
Once this is done, you now have a memory address, but we still must determine if it is a read or write. Do this with
randomness, but bias it towards reads. This information (the address requested and whether it is a read or write)
should be conveyed to oss. The user process will do a msgrcv waiting on a message back from oss. oss checks the
page reference by extracting the page number from the address, increments the clock as specified above, and sends
a message back.
At random times, say every 1000 ± 100 memory references, the user process will check whether it should terminate.
If so, all its memory should be returned to oss and oss should be informed of its termination.
The statistics of interest are:
 Number of memory accesses per second overall in the system
 Number of page faults per memory access */

#include "header.h"

#define _GNU_SOURCE

// Global variables
int shmid;
int msqid;
char logMessage[150];
struct SystemClock *sysClock;
struct msgbuf inbox, outbox;

/* INIT FUNCTIONS */

// Init shared memory
void initSharedMemory() {
    // Get shared memory
    shmid = shmget(SHMKEY, sizeof(struct SystemClock), IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("user_proc: Error: Failed to get shared memory");
        exit(EXIT_FAILURE);
    }
    
    // Attach shared memory
    sysClock = (struct SystemClock *)shmat(shmid, NULL, 0);
    if (sysClock == (void *)-1) {
        perror("user_proc: Error: Failed to attach shared memory");
        exit(EXIT_FAILURE);
    }
}

// Init message queue
void initMessageQueue() {
    // Get message queue
    msqid = msgget(MSGKEY, IPC_CREAT | 0666);
    if (msqid == -1) {
        perror("user_proc: Error: Failed to get message queue");
        exit(EXIT_FAILURE);
    }
}

// Main
int main(int argc, char *argv[]) {
    // Init shared memory
    initSharedMemory();
    
    // Init message queue
    initMessageQueue();
    
    // Loop
    while (1) {
        // Generate page
        int page = rand() % 32;

        // Generate offset
        int offset = rand() % 1024;

        // Add offset to page
        int address = (page * 1024) + offset;

        // Determine if read or write
        int readOrWrite = rand() % 100;
        if (readOrWrite < 85) {
            readOrWrite = 0;
        } else {
            readOrWrite = 1;
        }

        // Send message to oss with address and read/write
        outbox.mType = 1;
        outbox.mNum[0] = address;
        outbox.mNum[1] = readOrWrite;
        if (msgsnd(msqid, &outbox, sizeof(outbox.mNum), 0) == -1) {
            perror("user_proc: Error: Failed to send message to oss");
            exit(EXIT_FAILURE);
        }

        // Receive message from oss
        if (msgrcv(msqid, &inbox, sizeof(inbox.mNum), getppid(), 0) == -1) {
            perror("user_proc: Error: Failed to receive message from oss");
            exit(EXIT_FAILURE);
        }

        // Check if terminate every 1000 ± 100 memory references
        int terminate = rand() % 1100;
        if (terminate >= 900) {
            // Return all memory to oss (it detects this with a nonblocking waitpid)
            outbox.mType = getpid();
            outbox.mNum[0] = -1;
            outbox.mNum[1] = -1;
            if (msgsnd(msqid, &outbox, sizeof(outbox.mNum), 0) == -1) {
                perror("user_proc: Error: Failed to send message to oss");
                exit(EXIT_FAILURE);
            }
            
            // Detach shared memory
            if (shmdt(sysClock) == -1) {
                perror("user_proc: Error: Failed to detach shared memory");
                exit(EXIT_FAILURE);
            }
            
            printf("User process %d terminated\n", getpid());

            // Exit
            exit(EXIT_SUCCESS);
        }

    }
    
    return 0;
}