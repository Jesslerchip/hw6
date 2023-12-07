// Jessica Seabolt 11/17/2023 CMP_SCI 4760 Project 6

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
    inbox.mType = getpid();
    inbox.mData.pid = getpid();
    outbox.mType = 1;
}

// Main
int main(int argc, char *argv[]) {
    // Init shared memory
    initSharedMemory();
    
    // Init message queue
    initMessageQueue();

    // Seed random
    srand(getpid());
    
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
        outbox.mData.pid = getpid();
        outbox.mData.address = address;
        outbox.mData.readWrite = readOrWrite;

        if (msgsnd(msqid, &outbox, sizeof(outbox.mData), 0) == -1) {
            perror("user_proc: Error: Failed to send message to oss");
            exit(EXIT_FAILURE);
        }


        // Receive message from oss
        if (msgrcv(msqid, &inbox, sizeof(inbox.mData), getpid(), 0) == -1) {
            exit(EXIT_FAILURE);
        }

        // Check if terminate every 1000 ± 100 memory references
        int terminate = rand() % 1100;
        if (terminate >= 900) {
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