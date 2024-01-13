#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <string.h>
#include <fcntl.h>

struct Message {
    long int mType;
    int sqno;
    int opno;
    char filename[100];
};

struct Result {
    long mtype;
    char message[100];
    char output[100];   //not taking integer array because thenwe have to specify the size as well
};

struct SharedMemory {
    int numberOfNodes;
    int adjacencyMatrix[30][30]; // Assuming a maximum of 30 nodes
};

struct argument {
    struct Message message;
    int msqid;
};

struct nodeInfo {
    int n;
    int node;
    int adjacencyMatrix[30][30];
    int visited[30];
};

// Node structure for the linked list
struct Node {
    int data;
    struct Node* next;
};

// Queue structure
struct Queue {
    struct Node* front;
    struct Node* rear;
    int size;  
};

void initializeQueue(struct Queue* q) {
    q->front = NULL;
    q->rear = NULL;
    q->size = 0;
}

int isEmpty(struct Queue* q) {
    return q->front == NULL;
}

int size(struct Queue* q) {
    return q->size;
}

void enqueue(struct Queue* q, int value) {
    struct Node* newNode = (struct Node*)malloc(sizeof(struct Node));
    if (newNode == NULL) {
        fprintf(stderr, "Memory allocation error.\n");
        exit(EXIT_FAILURE);
    }
    newNode->data = value;
    newNode->next = NULL;

    if (isEmpty(q)) {
        q->front = newNode;
        q->rear = newNode;
    } 
    else {
        q->rear->next = newNode;
        q->rear = newNode;
    }

    q->size++;
}

int dequeue(struct Queue* q) {
    if (isEmpty(q)) {
        fprintf(stderr, "Queue is empty. Cannot dequeue.\n");
        exit(EXIT_FAILURE);
    }

    int value = q->front->data;

    struct Node* temp = q->front;
    q->front = q->front->next;

    if (q->front == NULL) {
        q->rear = NULL;
    }
    free(temp);
    q->size--;
    return value;
}


void displayQueue(struct Queue* q) {
    if (isEmpty(q)) {
        printf("Queue is empty.\n");
        return;
    }
    printf("Queue: ");
    struct Node* current = q->front;
    while (current != NULL) {
        printf("%d ", current->data);
        current = current->next;
    }
    printf("\n");
}

void freeQueue(struct Queue* q) {
    while (!isEmpty(q)) {
        dequeue(q);
    }
}

int deepestNodes[30];
int currentIndexDfs; 

int levelWiseNodes[30];
int currentIndexBfs; 

pthread_mutex_t deepestNodesMutex = PTHREAD_MUTEX_INITIALIZER;

void *dfs(void *arg) {
    struct nodeInfo *nodeinfo = (struct nodeInfo *)arg;
    int currentVertex = nodeinfo->node;
    nodeinfo->visited[currentVertex] = 1; 
    int flag = 0;
    
    pthread_t dfsThread[100]; //Assuming a maximum of 100 threads
    int dfsThreadCount = 0;
        	
    for (int i = 0; i < nodeinfo->n; ++i) {
        if (nodeinfo->adjacencyMatrix[currentVertex][i] == 1 && nodeinfo->visited[i] == 0) {
            flag = 1; // to check if leaf node
            
            struct nodeInfo *dfsnodeinfo = malloc(sizeof(struct nodeInfo));
            dfsnodeinfo->n = nodeinfo->n;
            dfsnodeinfo->node = i;
            memcpy(dfsnodeinfo->adjacencyMatrix, nodeinfo->adjacencyMatrix, sizeof(nodeinfo->adjacencyMatrix));
            memcpy(dfsnodeinfo->visited, nodeinfo->visited, sizeof(nodeinfo->visited));
            // MultiThreading:
            pthread_create(&dfsThread[dfsThreadCount], NULL, dfs, (void *)dfsnodeinfo);
            dfsThreadCount++;
        }
    }
    
    //waiting for all the Child threads
    for (int i = 0; i < dfsThreadCount; ++i) {
        pthread_join(dfsThread[i], NULL);
    }

    if (flag == 0) {
        // Use mutex to protect the critical section
        pthread_mutex_lock(&deepestNodesMutex);
        deepestNodes[currentIndexDfs] = currentVertex;
        currentIndexDfs++;
        pthread_mutex_unlock(&deepestNodesMutex);
    }

    free(arg);
    pthread_exit(NULL);
}

struct Queue q;
int visited[30];


void *fun(void *arg){
      struct nodeInfo *nodeinfo = (struct nodeInfo *)arg;
      for (int i = 0; i < nodeinfo->n; i++) {
            if (nodeinfo->adjacencyMatrix[nodeinfo->node][i] == 1 && !visited[i]) {
                enqueue(&q, i);
                visited[i] = 1;
            }
      }  
}   


void *bfs(void *arg) {

    struct nodeInfo *nodeinfo = (struct nodeInfo *)arg;
    int startNode = nodeinfo->node;

    
    initializeQueue(&q);

    enqueue(&q, startNode);
    visited[startNode] = 1;
    displayQueue(&q);
    while (!isEmpty(&q)) {
        int s = size(&q);
        pthread_t* bfsThread = malloc(s * sizeof(pthread_t));
        
        for(int i=0;i<s;i++){
            struct nodeInfo *bfsnodeinfo = malloc(sizeof(struct nodeInfo));
            bfsnodeinfo->n=nodeinfo->n;
            bfsnodeinfo->node = dequeue(&q);
            levelWiseNodes[currentIndexBfs]=bfsnodeinfo->node;
            currentIndexBfs++;
            memcpy(bfsnodeinfo->adjacencyMatrix, nodeinfo->adjacencyMatrix, sizeof(nodeinfo->adjacencyMatrix));
            pthread_create(&bfsThread[i], NULL, fun, (void *)bfsnodeinfo);   
        }
        for (int i = 0; i < s; ++i) {
            pthread_join(bfsThread[i], NULL);
        }
        displayQueue(&q);
    }
}

void *handleRead(void *arg) {
    struct argument Arg = *((struct argument *)arg);
    struct Message msg = Arg.message;

    FILE *file = fopen(msg.filename, "r");
    if (file == NULL) {
        perror("Error opening file");
        pthread_exit(NULL);
    }
    
    //finding out the number of node by reading the file recieved by client
    int numNodes;
    if (fscanf(file, "%d", &numNodes) != 1) {
        fprintf(stderr, "Error reading the number of nodes\n");
        fclose(file);
        pthread_exit(NULL);
    }
    printf("%d\n",numNodes);
    
    //initialising a 2D adjacency matrix
    int **readMatrix = (int **)malloc(numNodes * sizeof(int *));
    for (int i = 0; i < numNodes; i++) {
        readMatrix[i] = (int *)malloc(numNodes * sizeof(int));
    }
    
    //reading the adjacency matrix from the file
    for (int i = 0; i < numNodes; i++) {
        for (int j = 0; j < numNodes; j++) {
            fscanf(file, "%d", &readMatrix[i][j]);
        }
    }
    printf("Working with below Graph :\n");
    for (int i = 0; i < numNodes; i++) {
        for (int j = 0; j < numNodes; j++) {
            printf("%d ",readMatrix[i][j]);
        }
        printf("\n");
    }

    fclose(file);
    
    //Shared memory to get the starting vertex
    key_t shmKey = ftok("/mySharedMemory", 'B' + msg.sqno);
    int shmid = shmget(shmKey, sizeof(struct SharedMemory), 0666);
    if (shmid == -1) {
        perror("Error creating shared memory segment");
        exit(EXIT_FAILURE);
    }

    struct SharedMemory *sharedMemory = (struct SharedMemory *)shmat(shmid, NULL, 0);
    if (sharedMemory == (void *)-1) {
        perror("Error attaching shared memory segment");
        exit(EXIT_FAILURE);
    }
    
     //Including Semaphore
    char semaphoreName[50];
    sprintf(semaphoreName, "/my_semaphore_%d", msg.sqno);  // Unique semaphore name for each client
    sem_t *sem = sem_open(semaphoreName, O_CREAT, 0666, 1);

    // Check for errors in sem_open
    if (sem == SEM_FAILED) {
        perror("Error creating semaphore");
        exit(EXIT_FAILURE);
    }
    
    sem_wait(sem);
    int startingVertex = sharedMemory->numberOfNodes;
    sem_post(sem); // Release semaphore
    

    struct Result result;
    result.mtype = 8 * (msg.sqno) + msg.opno;
    currentIndexDfs = 0;
    currentIndexBfs=0;

    struct nodeInfo *nodeinfo = malloc(sizeof(struct nodeInfo));
    nodeinfo->n = numNodes;
    nodeinfo->node = startingVertex;
    
    for (int i = 0; i < numNodes; i++) {
        visited[i] = 0;
    }
    
    //copying readMatrix in dfsinfo
    for (int i = 0; i < nodeinfo->n; i++) {
	    for (int j = 0; j < nodeinfo->n; j++) {
            nodeinfo->adjacencyMatrix[i][j]=readMatrix[i][j];
        }
	    nodeinfo->visited[i] = visited[i];
    } 
    
    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);
    pthread_mutex_lock(&mutex); 

    if (msg.opno == 3){ 
        pthread_t mainThread;
        pthread_create(&mainThread, NULL, dfs, (void *)nodeinfo);
        pthread_join(mainThread, NULL);

        char answer[100];
        int curIdx = 0;
	
	    //converting the deepestNodes array into a character array
        for (int i = 0; i < currentIndexDfs; ++i) {
            curIdx += snprintf(answer + curIdx, sizeof(answer) - curIdx, "%d", deepestNodes[i]+1);
            if (i < currentIndexDfs - 1) {
                answer[curIdx++] = ' ';
            }
        }
        answer[curIdx] = '\0';
	
	    //reinitialising the deepestNodes to 0 and index to 0
        memset(deepestNodes, 0, sizeof(deepestNodes));
        currentIndexDfs = 0;

        // Copy the answer to result.output
        strncpy(result.output, answer, sizeof(result.output) - 1);
        result.output[sizeof(result.output) - 1] = '\0';
    } 
    else if(msg.opno == 4) {
        pthread_t mainThread;
        pthread_create(&mainThread, NULL, bfs, (void *)nodeinfo);
        pthread_join(mainThread, NULL);

        char answer[100];
        int curIdx = 0;
	
	    //converting the deepestNodes array into a character array
        for (int i = 0; i < currentIndexBfs; ++i) {
            curIdx += snprintf(answer + curIdx, sizeof(answer) - curIdx, "%d", levelWiseNodes[i]+1);
            if (i < currentIndexBfs - 1) {
                answer[curIdx++] = ' ';
            }
        }
        answer[curIdx] = '\0';
	
	    //reinitialising the deepestNodes to 0 and index to 0
        memset(levelWiseNodes, 0, sizeof(levelWiseNodes));
        currentIndexBfs = 0;

        // Copy the answer to result.output
        strncpy(result.output, answer, sizeof(result.output) - 1);
        result.output[sizeof(result.output) - 1] = '\0';
    }
    

    if (shmdt(sharedMemory) == -1) {
        perror("Error detaching shared memory segment");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < numNodes; i++) {
        free(readMatrix[i]);
    }
    free(readMatrix);

    if (msgsnd(Arg.msqid, &result, sizeof(struct Result), 0) == -1) {
        perror("Error sending message to the Client");
        exit(EXIT_FAILURE);
    }
    pthread_mutex_unlock(&mutex);
    pthread_mutex_destroy(&mutex);
    pthread_exit(NULL);
}

int main() {
    int run1=1,run2=1;
    int serverNumber;
    printf("Enter server Number : ");
    scanf("%d",&serverNumber);
    key_t msgQueueKey = ftok("/myMsgQueue", 'A'); // Unique key for the message queue
    int msqid = msgget(msgQueueKey, 0666);
    if (msqid == -1) {
        perror("Error creating message queue");
        exit(EXIT_FAILURE);
    }
    
    pthread_t threads[100]; 
    int threadCount = 0;

    while (1) {
        struct Message msg;
        
        if (msgrcv(msqid, &msg, sizeof(msg), 3, 0) == -1) {
            perror("Error receiving from message queue");
            exit(EXIT_FAILURE);
        }
        if (msg.opno == 5) {
            printf("CleanUp Started.");
            // Cleanup request received
            break;
        }
        struct argument *arg = malloc(sizeof(struct argument));
        arg->message = msg;
        arg->msqid = msqid;
        
        
        if (msg.sqno % 2 == 0) {
            if (serverNumber == 2 && run2) {
                if (msg.opno == 5) {
                    run2=0;
                    printf("CleanUp Started.\n");                    
                }   
                else{
                    printf("Serving client with sqno : %d\n",msg.sqno);
                    pthread_create(&threads[threadCount], NULL, handleRead, (void *)arg);
                    threadCount++;
                }
            } 
            else {printf("Skipping even sqno request.\n");}
        } 
        else {
            if (serverNumber == 1 && run1) {
                if (msg.opno == 5) {
                    run1=0;
                    printf("CleanUp Started.\n");                    
                }
                else{
                    printf("Serving client with sqno : %d\n",msg.sqno);
                    pthread_create(&threads[threadCount], NULL, handleRead, (void *)arg);
                    threadCount++;
                }
            } 
            else {printf("Skipping odd sqno request.\n");}
        }
        if(run1==0 && run2==0) break;
        // break if the threadCount exceeds the maximum allowed threads
        if (threadCount >= 100) {
            fprintf(stderr, "Maximum thread limit reached. Exiting.\n");
            break;
        }
    }
    
    for (int i = 0; i < threadCount; ++i) {
        pthread_join(threads[i], NULL);
    }
    

    return 0;
}

