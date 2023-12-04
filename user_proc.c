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
    
    // Init random number generator
    srand(getpid());
    
    // Get process ID
    int pid = getpid();

    // Check the clock
    printf("user_proc: %d: %u:%u\n", pid, sysClock->seconds, sysClock->nanoseconds);

    // TODO: Verify it has the correct resource limit
    
    // Send message to oss
    outbox.mType = pid;
    outbox.mNum = 123456789;
    if (msgsnd(msqid, &outbox, sizeof(outbox), 0) == -1) {
        perror("user_proc: Error: Failed to send message to oss");
        exit(EXIT_FAILURE);
    }
    
    // Detach shared memory
    if (shmdt(sysClock) == -1) {
        perror("user_proc: Error: Failed to detach shared memory");
        exit(EXIT_FAILURE);
    }
    
    return 0;
}