//
// Created by fedya on 23.10.17.
//

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <locale.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <time.h>
#include <sys/time.h>

#define PAGE_SIZE (16 * 1024)
#define OTL_DES 10
#define N_SEM 8

int i = 0;

int to_null(int * i){
    int a = *i;
    *i = 0;
    return a;
}

#define SEM(num, op, flg)             \
semaphores[i].sem_num = num;           \
semaphores[i].sem_op = op;              \
semaphores[i++].sem_flg = flg;           \

#define SEMOP() semop(semid, semaphores, to_null(&i));

union semun {
    int              val;    /* Value for SETVAL */
    struct semid_ds *buf;    /* Buffer for IPC_STAT, IPC_SET */
    unsigned short  *array;  /* Array for GETALL, SETALL */
    struct seminfo  *__buf;  /* Buffer for IPC_INFO (Linux-specific) */
};

static const int MOYA_OTSENOCHKA = OTL_DES;

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
static sem_buf semaphores [N_SEM] = {};

static int Write_To_Shmem(char * source_file, char * buf, int semid);
static int Read_From_Shmem(char * buf, int semid);
static key_t Get_Key();
int Print_Sems(int semid);

#include "print_sems.h"

int main(int argc, char **argv) {
    key_t key = Get_Key();
    //printf("key %d\n", key);

    int shmid = shmget(key, PAGE_SIZE + sizeof(ssize_t), 0);
    if (shmid == -1) {
        if (errno != ENOENT){
            perror("shmget1");
            exit(EXIT_FAILURE);
        }
        shmid = shmget(key, PAGE_SIZE + sizeof(ssize_t), IPC_CREAT | 0666);
        if (shmid == -1) {
            perror("shmget2");
            exit(EXIT_FAILURE);
        }
    }
    char * buf = shmat(shmid, NULL, 0);
    if (buf == (void *) -1){
        perror("shmat");
        exit(EXIT_FAILURE);
    }
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

    if (argc == 2)
        Write_To_Shmem(argv[1], buf, semid);
    else if (argc == 1)
        Read_From_Shmem(buf, semid);
    else {
        perror("Bad number of arguments");
        exit(EXIT_FAILURE);
    }
    return EXIT_SUCCESS;
}

static int Write_To_Shmem(char * source_file, char * buf, int semid){
    int fd_source;
    if ((fd_source = open(source_file, O_RDONLY)) == -1) {
        perror("source file");
        exit(EXIT_FAILURE);
    }
    //Print_Sems(semid);
    SEM(WRITER_ALIVE, 0, 0);                         //wait if a writer already exists
    SEM(WRITER_ALIVE, 1, SEM_UNDO);                  //make everybody know that the writer exists
    SEMOP();


    SEM(READER_ALIVE, -1, 0);                        //wait for an another
    SEM(READER_ALIVE, 1, 0);
    SEM(ZBS, 0, 0);                                  //if ZBS is 1 then the old another chel is alive
    SEM(ZBS2, 1, SEM_UNDO);
    SEMOP();                                         //Critical region being handled by first semop
                                                     // (not to let another writer enter this code)
 
    SEM(ZBS2, -2, 0);
    SEM(ZBS2, 2, 0);
    SEMOP();

    union semun init;                                //we need to re-initialize our sems
    init.val = 1;
    semctl(semid, EMPTY, SETVAL, init);              //Critical section starts (initialization)
    semctl(semid, MUTEX, SETVAL, init);
    init.val = 0;
    semctl(semid, FULL, SETVAL, init);
    memset(buf, 0, PAGE_SIZE + sizeof(ssize_t));
    SEM(ZBS, 1, SEM_UNDO);
    SEM(ZBS, -2, 0);
    SEM(ZBS, 2, 0);
    SEMOP();                                         //Critical region being handled by zbs2
                                                     //We can't let him to set zbs because another can think
                                                     //that the previous writer is alive and will be stuck on sem(zbs, 0, 0)
    /*printf("Kill me pliz\n");
    getchar();*/
    while (1) {
        //Print_Sems(semid);
        SEM(ZBS, -2, IPC_NOWAIT);
        SEM(ZBS, 2, 0);
        SEM(EMPTY, -1, SEM_UNDO);
        //SEMOP();
        SEM(FULL, 1, 0);
        SEM(FULL, -1, SEM_UNDO);                                //Not to make the reader wait forever
        SEM(MUTEX, -1, SEM_UNDO);
        int res = SEMOP();                                      //from this (mutex)
        if (res == -1){
            if (errno == EAGAIN){
                printf("Reader is dead\n");
                exit(EXIT_SUCCESS);
            }
            perror("semop");
            exit(EXIT_FAILURE);
        }
        /*puts("1");
        getchar();*/
        ssize_t have_read = read(fd_source, buf, PAGE_SIZE);    //Critical section (shmem)
        *(ssize_t *) (buf + PAGE_SIZE) = have_read;

        /*puts("2");
        getchar();*/
        SEM(MUTEX, 1, SEM_UNDO);
        SEM(FULL, 1, 0);
        SEMOP();                                                //To this (mutex)
        if (have_read < 0) {
            perror("read");
            exit(EXIT_FAILURE);
        }
        if (have_read < PAGE_SIZE)
            break;
    }
    //Print_Sems(semid);
    SEM(ZBS, -1, 0);
    SEM(ZBS, 0, 0);		                                        //wait not to die earlier that the reader
    SEM(ZBS, 1, 0);
    SEMOP();
    //Critical region starts
    if (shmdt(buf)){
        perror("shmdt");
    }
    return EXIT_SUCCESS;
}

static int Read_From_Shmem(char * buf, int semid) {
    SEM(READER_ALIVE, 0, 0);                                    //wait if a reader already exists
    SEM(READER_ALIVE, 1, SEM_UNDO);                             //make everybody know that the writer exists
    SEMOP();

    SEM(WRITER_ALIVE, -1, 0);                                   //wait for an another
    SEM(WRITER_ALIVE, 1, 0);
    SEM(ZBS, 0, 0);
    SEM(ZBS2, 1, SEM_UNDO);                                     //if ZBS is 1 then old another chel is alive
    SEMOP();

    SEM(ZBS2, -2, 0);
    SEM(ZBS2, 2, 0);
    SEMOP();

    SEM(ZBS, 1, SEM_UNDO);
    SEMOP();
    /*printf("Don't kill me\n");
    getchar();*/
    SEM(WRITER_ALIVE, -1, IPC_NOWAIT)
    SEM(WRITER_ALIVE, 1, 0)
    SEM(ZBS, -2, 0);
    SEM(ZBS, 2, 0);
    int res = SEMOP();
    if (res == -1){
        if (errno == EAGAIN){
            printf("Writer has died\n");
            exit(EXIT_FAILURE);
        }
        perror("semop");
        exit(EXIT_FAILURE);
    }

    while (1) {
        //Print_Sems(semid);
        SEM(ZBS, -2, IPC_NOWAIT);
        SEM(ZBS, 2, 0);
        SEM(FULL, -1, SEM_UNDO);
        SEM(EMPTY, 1, 0);
        SEM(EMPTY, -1, SEM_UNDO);                               //Not to make the writer wait forever
        SEM(MUTEX, -1, SEM_UNDO);
        res = SEMOP();
        if (res == -1){
            if (errno == EAGAIN){
                printf("Writer is dead\n");
                exit(EXIT_SUCCESS);
            }
            perror("semop");
            exit(EXIT_FAILURE);
        }
        /*puts("1");
        getchar();*/
        ssize_t to_write = *(ssize_t *) (buf + PAGE_SIZE);
        ssize_t have_printed = write(1, buf, (size_t) to_write);
        fflush(stdout);
        SEM(MUTEX, 1, SEM_UNDO);
        SEM(EMPTY, 1, 0);
        SEMOP();
        if (to_write < PAGE_SIZE){
            break;
        }
        if (have_printed < 0) {
            exit(EXIT_FAILURE);
        }
    }
    if (shmdt(buf)){
        perror("shmdt");
    }
    return EXIT_SUCCESS;
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
