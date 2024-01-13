#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <fcntl.h>

/*
mtype | opno | sender        | reciever     	      |
_______________________________________________________
1     | any  | client        | load_balancer          |
2     | any  | load_balancer | primary_server(service)|
2     | 5    | load_balancer | primary_server(cleanup)|
3     | any  | load_balancer | sec_server(service)    |
3     | 5    | load_balancer | sec_server(cleanup)    |
1     | 5    | cleanup       | load_balancer          |
*/

struct Message {
    long mType;
    int sqno;
    int opno;
    char filename[100];
};

int main() {
    key_t msgQueueKey = ftok("/myMsgQueue", 'A'); // Unique key for the message queue
    int msqid = msgget(msgQueueKey, IPC_CREAT | 0666);
    if (msqid == -1) {
        perror("Error creating message queue");
        exit(EXIT_FAILURE);
    }

    struct Message msg;
    printf("Load Balancer Started\n");

    while (1) {
        if (msgrcv(msqid, &msg, sizeof(msg), 1, 0) == -1) { 
            perror("Error receiving from message queue");
            exit(EXIT_FAILURE);
        }
        printf("Request received from client with sqno : %d\n",msg.sqno);

        if (msg.opno == 5) {
            // Termination request from cleanup
            // Sending termination request to all the servers
            // Sending to primary server
            struct Message msg1;
            msg1.mType = 2;
            msg1.opno = 5;
            if (msgsnd(msqid, &msg1, sizeof(struct Message), 0) == -1) {
                perror("Error sending message to the primary server");
                exit(EXIT_FAILURE);
            }

            // Sending to secondary server 1
            struct Message msg2; 
            msg2.mType = 3;
            msg2.opno = 5;
            if (msgsnd(msqid, &msg2, sizeof(struct Message), 0) == -1) {
                perror("Error sending message to the secondary server");
                exit(EXIT_FAILURE);
            }
            
            // Sending to secondary server 2
            struct Message msg3; 
            msg3.mType = 3;
            msg3.opno = 5;
            if (msgsnd(msqid, &msg3, sizeof(struct Message), 0) == -1) {
                perror("Error sending message to the secondary server");
                exit(EXIT_FAILURE);
            }
            

            sleep(5);

            // Delete the message queue
            if (msgctl(msqid, IPC_RMID, NULL) == -1) { //cleanup
                perror("Error deleting message queue");
                exit(EXIT_FAILURE);
            } 
            else {
                printf("Message queue deleted successfully\n");
                break; // Exit the main loop after deleting the message queue
            }
        } 
        else {
            if (msg.opno == 1 || msg.opno == 2) { // write
                msg.mType = 2;
                printf("Sending request to Primary server\n");
                if (msgsnd(msqid, &msg, sizeof(msg), 0) == -1) {
                    perror("Error sending message to the primary server");
                    exit(EXIT_FAILURE);
                }
            } 
            else if (msg.opno == 3 || msg.opno == 4) { // read
                msg.mType = 3;
                printf("Sending request to Secondary server\n");
                if (msgsnd(msqid, &msg, sizeof(struct Message), 0) == -1) {
                    perror("Error sending message to the secondary server");
                    exit(EXIT_FAILURE);
                }
            } 
            else {// continue
            }
        }
    }
    return 0;
}

