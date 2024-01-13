#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <semaphore.h>

struct Message {
    long mType;
    int sqno;
    int opno;
    char filename[100];
};

struct SharedMemory {
    int numberOfNodes;
    int adjacencyMatrix[30][30]; // Assuming a maximum of 30 nodes
};

struct Result {
    long mtype;
    char message[100];
    char output[100];
};

struct argument {
    struct Message message;
    int msqid;
};

void *handleWrite(void *arg) {
    struct argument Arg = *((struct argument *)arg);
    struct Message msg = Arg.message;

    key_t shmKey = ftok("/mySharedMemory", 'B' + msg.sqno);
    int shmid = shmget(shmKey, sizeof(struct SharedMemory), 0666);

    if (shmid == -1) {
        perror("Error creating shared memory segment");
        exit(EXIT_FAILURE);
    }

    // Attach shared memory segment to the process
    struct SharedMemory *sharedMemory = (struct SharedMemory *)shmat(shmid, NULL, 0);

    if (sharedMemory == (void *)-1) {
        perror("Error attaching shared memory segment");
        exit(EXIT_FAILURE);
    }

    // Including Semaphore
    char semaphoreName[50];
    sprintf(semaphoreName, "/my_semaphore_%d", msg.sqno); // Unique semaphore name for each client(imp)
    sem_t *sem = sem_open(semaphoreName, O_CREAT, 0666, 1);

    // Check for errors in sem_open
    if (sem == SEM_FAILED) {
        perror("Error creating semaphore");
        exit(EXIT_FAILURE);
    }

    struct Result result;
    result.mtype = 8 * (msg.sqno) + msg.opno;

    int option = msg.opno;
    FILE *file;

    if (option == 1) {
        sem_wait(sem); // Wait for semaphore

        file = fopen(msg.filename, "w");

        if (file == NULL) {
            perror("Error opening file");
            return 0;
        }

        fprintf(file, "%d\n", sharedMemory->numberOfNodes);

        for (int i = 0; i < sharedMemory->numberOfNodes; ++i) {
            for (int j = 0; j < sharedMemory->numberOfNodes; ++j) {
                fprintf(file, "%d ", sharedMemory->adjacencyMatrix[i][j]);
            }
            fprintf(file, "\n");
        }

        strcpy(result.message, "File successfully added");
        fclose(file);
        sem_post(sem); // Release semaphore
    } 
    else if (option == 2) {
        sem_wait(sem); // Wait for semaphore

        file = fopen(msg.filename, "r+");

        if (file == NULL) {
            perror("Error opening file");
            return 0;
        }

        if (ftruncate(fileno(file), 0) != 0) {
            perror("Error truncating file");
            fclose(file);
            return 0;
        }

        // Set the file pointer to the beginning
        fseek(file, 0, SEEK_SET);

        fprintf(file, "%d\n", sharedMemory->numberOfNodes);

        for (int i = 0; i < sharedMemory->numberOfNodes; ++i) {
            for (int j = 0; j < sharedMemory->numberOfNodes; ++j) {
                fprintf(file, "%d ", sharedMemory->adjacencyMatrix[i][j]);
            }
            fprintf(file, "\n");
        }

        strcpy(result.message, "File successfully modified");
        fclose(file);
        sem_post(sem); // Release semaphore
    } 
    else {
        printf("Invalid Request received\n");
    }

    sem_close(sem); // Close the semaphore

    if (msgsnd(Arg.msqid, &result, sizeof(struct Result), 0) == -1) {
        perror("Error sending message to the Client");
        exit(EXIT_FAILURE);
    }

    close(shmid);

    pthread_exit(NULL);
}

int main() {
    key_t msgQueueKey = ftok("/myMsgQueue", 'A'); // Unique key for the message queue
    int msqid = msgget(msgQueueKey, 0666);

    if (msqid == -1) {
        perror("Error creating message queue");
        exit(EXIT_FAILURE);
    }

    pthread_t threads[100]; // Assuming a maximum of 100 threads
    int threadCount = 0;

    while (1) {
        struct Message msg;
        if (msgrcv(msqid, &msg, sizeof(msg), 2, 0) == -1) {
            perror("Error receiving from message queue");
            exit(EXIT_FAILURE);
        }

        if (msg.opno == 5) {
            // Cleanup request received
            break;
        }
        printf("Serving client with sqno : %d\n",msg.sqno);

        struct argument *arg = malloc(sizeof(struct argument));
        arg->message = msg;
        arg->msqid = msqid;

        pthread_create(&threads[threadCount], NULL, handleWrite, (void *)arg);
        threadCount++;

        // break If the threadCount exceeds the maximum allowed threads
        if (threadCount >= 100) {
            fprintf(stderr, "Maximum thread limit reached. Exiting.\n");
            break;
        }
    }

    // Wait for all threads to finish
    for (int i = 0; i < threadCount; ++i) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}

