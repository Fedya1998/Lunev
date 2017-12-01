//
// Created by fedya on 03.11.17.
//

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#define NDEBUG 10
#include <my_debug.h>
#include <sys/time.h>
#include <locale.h>

static int Write(char * source_file);
static int Read();
static void OnSigchld(int sig);
static int SendSig(pid_t dest, int sig);
static void OnUSR(int sig);
static void Alarm(int signal);
static void FileEnd(int sig);

static pid_t parent;
static pid_t child;
static sigset_t set;
static int bit = 0;

static char FUNC[20] = "";
static int LINE = 0;

#define Wait()                        \
    alarm(1);                          \
    LINE = __LINE__;                    \
    sigsuspend(&set);    /*crit*/        \
    alarm(0);

int main(int argc, char ** argv) {
    setlocale(LC_ALL, "");
    if (argc != 2) {
        printf("Wrong argument count\n");
        return EXIT_FAILURE;
    }

    struct sigaction act = {.sa_handler = OnSigchld};
    sigaction(SIGCHLD, &act, NULL);
    act.sa_handler = OnUSR;
    sigaction(SIGUSR1, &act, NULL);
    sigaction(SIGUSR2, &act, NULL);
    act.sa_handler = Alarm;
    sigaction(SIGALRM, &act, NULL);
    sigfillset(&set);
    sigdelset(&set, SIGCHLD);
    sigdelset(&set, SIGALRM);
    sigdelset(&set, SIGINT);
    sigprocmask(SIG_SETMASK, &set, NULL);
    sigdelset(&set, SIGUSR1);
    sigdelset(&set, SIGUSR2);
    sigdelset(&set, SIGSYS);
    act.sa_handler = FileEnd;
    sigaction(SIGSYS, &act, NULL);
    parent = getpid();
    pid_t pid = fork();
    if (pid < 0){
        perror("fork");
        exit(EXIT_FAILURE);
    }
    else if (pid > 0){
        child = pid;
        return Read();
    }
    else if (pid == 0){
        return Write(argv[1]);
    }
    return 0;
}

static int Write(char * source_file) {
    strcpy(FUNC, __FUNCTION__);
    int fd = open(source_file, O_RDONLY);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }
    while(1){
        char byte = 0;
        ssize_t have_read = read(fd, &byte, 1);
        //printf("byte to send %d\n", byte);
        if (have_read == 0) {
            Wait();
            SendSig(parent, SIGSYS);              //-----
            Wait();
            break; //crit
        }
        else {
            for (int i = 0; i < 8; i++) {
                Wait();
                if ((byte | (1 << i)) == byte)
                    SendSig(parent, SIGUSR1);      //crit
                else
                    SendSig(parent, SIGUSR2);      //crit
            }
        }
        if (have_read == -1){
            perror("read");
            exit(EXIT_FAILURE);
        }
    }
    Gde;
    return EXIT_SUCCESS;
}

static int Read(){
    strcpy(FUNC, __FUNCTION__);
    while (1){
        char byte = 0;
        for (int i = 0; i < 8; i++){
            SendSig(child, SIGUSR1);    //crit
            sigsuspend(&set);
            if (bit)                    //crit
                byte |= 1 << i;
        }
        write(1, &byte, 1);
        Gde;
    }
    Gde;
    return EXIT_SUCCESS;
}

static void Alarm(int signal){
    //printf("alarm line %d\n", LINE);
    exit(EXIT_FAILURE);
}

static void OnUSR(int sig){
    switch (sig){
        case SIGUSR1:
            bit = 1;
            return;
        case SIGUSR2:
            bit = 0;
            return;
        default:
            exit(EXIT_FAILURE);
    }
}

static int SendSig(pid_t dest, int sig){
    p(10, ("%s sending sig %d\n", FUNC, sig));
    int res = kill(dest, sig);
    if (res == -1){
        if (errno == ESRCH){
            printf("I'm %s, that side is dead\n", FUNC);
            exit(EXIT_SUCCESS);
        }
        perror("sig");
        exit(EXIT_FAILURE);
    }
    return res;
}

static void OnSigchld(int sig){
    //printf("Writer is dead, I'm dying too\n");
    exit(EXIT_FAILURE);
}

static void FileEnd(int sig){
    //SendSig(parent, SIGUSR1);
    exit(EXIT_SUCCESS);
}