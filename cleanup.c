#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>

struct Message {
    long mType;
    int sqno;
    int opno;
    char filename[100];
};

int main() {
    key_t msgQueueKey = ftok("/myMsgQueue", 'A'); // Unique key for the message queue

    int msqid = msgget(msgQueueKey, 0666);

    if (msqid == -1) {
        perror("Error creating message queue");
        exit(EXIT_FAILURE);
    }

    while (1) {
        char choice;

        printf("Want to terminate the application? Press Y (Yes) or N (No)\n");
        scanf(" %c", &choice);

        if (choice == 'Y' || choice == 'y') {
            struct Message msg;
            msg.mType = 1;
            msg.opno = 5; // used to clarify to the server that it's cleanup and not client

            // Inform load balancer to terminate
            if (msgsnd(msqid, &msg, sizeof(struct Message), 0) == -1) {
                perror("msgsnd");
                exit(EXIT_FAILURE);
            }

            printf("Cleanup process terminated.\n");
            exit(EXIT_SUCCESS);
        } 
        else if (choice == 'N' || choice == 'n') {
            // Continue running as usual
        } 
        else {
            printf("Invalid choice. Please enter Y or N.\n");
        }
    }

    return 0;
}

