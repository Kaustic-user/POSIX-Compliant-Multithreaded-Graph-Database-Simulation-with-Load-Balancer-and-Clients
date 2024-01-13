#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <fcntl.h>

// Message structure for communication between client and servers
struct Message {
    long mType;
    int sqno;
    int opno;
    char filename[100];
};

// Structure for the client to receive back the result
struct Result {
    long mtype;
    char message[100];
    char output[100];
};

// Structure for shared memory segment
struct SharedMemory {
    int numberOfNodes;
    int adjacencyMatrix[30][30]; // Assuming a maximum of 30 nodes
};

int main() {
    key_t msgQueueKey = ftok("/myMsgQueue", 'A'); // Unique key for the message queue
    int msqid = msgget(msgQueueKey, 0666); //noipccreat

    if (msqid == -1) {
        perror("Error creating message queue");
        exit(EXIT_FAILURE);
    }

    struct Message message;
    int sequenceNumber, operationNumber;
    char graphFileName[50];

    while (1) {
        // Display menu options
        printf("Menu Options:\n");
        printf("1. Add a new graph to the database\n");
        printf("2. Modify an existing graph of the database\n");
        printf("3. Perform DFS on an existing graph of the database\n");
        printf("4. Perform BFS on an existing graph of the database\n");

        // Get user input
        printf("Enter Sequence Number: ");
        scanf("%d", &sequenceNumber);

        printf("Enter Operation Number : ");
        scanf("%d", &operationNumber);

        printf("Enter Graph File Name: ");
        scanf("%s", graphFileName);

        // Prepare the message
        message.mType = 1;
        message.sqno = sequenceNumber;
        message.opno = operationNumber;
        strcpy(message.filename, graphFileName);

        // Unique key for shared memory
        key_t shmKey = ftok("/mySharedMemory", 'B' + sequenceNumber); // Unique shared memory for each client(imp)
        int shmid = shmget(shmKey, sizeof(struct SharedMemory), IPC_CREAT | 0666);
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
        sprintf(semaphoreName, "/my_semaphore_%d", sequenceNumber); // Unique semaphore name for each client(imp)
        sem_t *sem = sem_open(semaphoreName, O_CREAT, 0666, 1);

        // Check for errors in sem_open
        if (sem == SEM_FAILED) {
            perror("Error creating semaphore");
            exit(EXIT_FAILURE);
        }
        sem_wait(sem); // Wait for semaphore

        // Send the message to the load balancer
        if (msgsnd(msqid, &message, sizeof(message), 0) == -1) {
            perror("Error sending message to the load balancer");
            exit(EXIT_FAILURE);
        }

        // Handle write operation
        if (operationNumber == 1 || operationNumber == 2) {
            printf("Enter number of nodes of the graph: ");
            scanf("%d", &sharedMemory->numberOfNodes);

            printf("Enter adjacency matrix:\n");
            for (int i = 0; i < sharedMemory->numberOfNodes; i++) {
                for (int j = 0; j < sharedMemory->numberOfNodes; j++) {
                    scanf("%d", &sharedMemory->adjacencyMatrix[i][j]);
                }
            }
        }

        // Handle read operation
        if (operationNumber == 3 || operationNumber == 4) {
            int startingVertex;
            printf("Enter starting vertex: ");
            scanf("%d", &startingVertex);

            // Write starting vertex to shared memory in the numberOfNodes
            sharedMemory->numberOfNodes = startingVertex - 1;
        }

        sem_post(sem); // Release semaphore

        // Receive from message queue
        struct Result result;

        if (msgrcv(msqid, &result, sizeof(struct Result), 8 * sequenceNumber + operationNumber, 0) == -1) {
            perror("Error receiving from message queue");
            exit(EXIT_FAILURE);
        }

        if (operationNumber == 1 || operationNumber == 2) {
            printf("%s\n", result.message);
        } 
        else {
            printf("%s\n", result.output);
        }

        // Detach shared memory segment
        if (shmdt(sharedMemory) == -1) {
            perror("Error detaching shared memory segment");
            exit(EXIT_FAILURE);
        }

        shmctl(shmid, IPC_RMID, NULL); // delete the shared memory
        sem_close(sem);                // Close the semaphore
    }

    return 0;
}

