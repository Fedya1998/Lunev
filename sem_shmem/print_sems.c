//
// Created by fedya on 31.10.17.
//
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <locale.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/time.h>
#include <time.h>

#define PAGE_SIZE (16 * 1)
#define OTL_DES 10
#define N_SEM 8

#define SEM(i, num, op, flg)          \
semaphores[i].sem_num = num;           \
semaphores[i].sem_op = op;              \
semaphores[i].sem_flg = flg;             \

#define SEMOP(num) semop(semid, semaphores, num)

union semun {
    int              val;    /* Value for SETVAL */
    struct semid_ds *buf;    /* Buffer for IPC_STAT, IPC_SET */
    unsigned short  *array;  /* Array for GETALL, SETALL */
    struct seminfo  *__buf;  /* Buffer for IPC_INFO (Linux-specific) */
};

const int MOYA_OTSENOCHKA = OTL_DES;

enum sem_names {
#define sem(NAME) NAME,
#include "sems.txt"
#undef sem
    SEM_EMPTY
};

const char * names[N_SEM + 1] = {
#define sem(NAME) #NAME,
#include "sems.txt"
#undef sem
        "SEM_EMPTY"
};

typedef struct sembuf sem_buf;
sem_buf semaphores [N_SEM] = {};

static key_t Get_Key();
int Print_Sems(int semid);

#include "print_sems.h"

int main(int argc, char ** argv){
    key_t key = Get_Key();
    int semid = semget(key, N_SEM, 0);
    if (semid == -1) {
        if (errno != ENOENT){
            perror("semget1");
            exit(EXIT_FAILURE);
        }
        semid = semget(key, N_SEM, IPC_CREAT | 0666);
        if (semid == -1) {
            perror("semget2");
            exit(EXIT_FAILURE);
        }
    }
    Print_Sems(semid);
}



static key_t Get_Key() {
    if (creat("/home/fedya/code/Lunev/sem_shmem/file", 0666) == -1){
        if (errno != EEXIST)
            exit(EXIT_FAILURE);
    }
    key_t key = ftok("/home/fedya/code/Lunev/sem_shmem/file", MOYA_OTSENOCHKA);
    if (key == -1){
        perror("ftok");
        exit(EXIT_FAILURE);
    }
    return key;
}
