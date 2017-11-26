#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <limits.h>
#include <sys/msg.h>
#include <sys/ipc.h>

typedef struct msg_buf {
    long mtype;
} msg_buf;

long my_strtol(char *str);


int main(int argc, char *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "Wrong argument count!\n");
        return EXIT_FAILURE;
    }

    long n = my_strtol(argv[1]);

    if (!n)
        exit(EXIT_SUCCESS);

    struct msg_buf msg = {0};

    int msgq = msgget(IPC_PRIVATE, IPC_CREAT | 0666);
    if (msgq < 0) {
        perror("queue");
        exit(EXIT_FAILURE);
    }

    pid_t pid = -1;
    for (long i = 1; i <= n; i++) {
        pid = fork();
        if (pid == 0) {
            if (i == 1)
                goto print;
            if (msgrcv(msgq, &msg, 0, i - 1, 0) < 0) {
                perror("rcv");
                exit(EXIT_FAILURE);
            }
            print:
            printf("%ld ", i);
            fflush(stdout);
            msg.mtype++;
            if (msgsnd(msgq, &msg, 0, 0) < 0) {
                perror("snd");
                exit(EXIT_FAILURE);
            }
            exit(EXIT_SUCCESS);
        }
        else if (pid < 0) {
            exit(EXIT_FAILURE);
        }
    }

    if (msgrcv(msgq, &msg, 0, n, 0) < 0) {
        perror("rcv");
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}

long my_strtol(char *str){
    char * endptr;
    long val = strtol(str, &endptr, 10);


    if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN))
        || (errno != 0 && val == 0)) {
        perror("strtol");
        exit(EXIT_FAILURE);
    }

    if (endptr == str) {
        fprintf(stderr, "No digits were found\n");
        exit(EXIT_FAILURE);
    }

    if (*endptr != '\0')        /* Not necessarily an error... */{
        printf("Not a number\n");
        exit(EXIT_FAILURE);
    }


    if (val < 1){
        printf("Wrong input");
        exit(EXIT_FAILURE);
    }

    return val;
}