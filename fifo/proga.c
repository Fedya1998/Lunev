//
// Created by fedya on 11.10.17.
//

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#define DEBUG 10

#include <my_debug.h>
#include <math.h>
#define PAGE_SIZE (16 * 1024)
#define NAME_SIZE 30

int Reader_Exists(int name, char * source_file);

int Write_To_Fifo(char *source_file);

int Read_From_Fifo();

int Define_Fifo_Name(char *name);


int main(int argc, char **argv) {
    if (argc == 2)
        Write_To_Fifo(argv[1]);
    else if (argc == 1)
        Read_From_Fifo();
    else {
        perror("Bad arguments count");
        exit(EXIT_FAILURE);
    }
    return EXIT_SUCCESS;
}

int Write_To_Fifo(char *source_file) {
    Gde;
    int fd_main_fifo;
    if ((fd_main_fifo = open("main_fifo", O_RDONLY)) == -1) {
        if (errno != ENOENT) {
            perror("main_fifo");
            exit(EXIT_FAILURE);
        }
        mkfifo("main_fifo", 0666);
        fd_main_fifo = open("main_fifo", O_RDONLY);
    }

    int val;
    switch (/*critical region starts*/read(fd_main_fifo, &val, sizeof(val))) {
        case 0:
            printf("Reader has closed\n");
            exit(EXIT_SUCCESS);
        case sizeof(val):
            if (val <= 0) {
                printf("bad val");
                exit(EXIT_FAILURE);
            }
            Reader_Exists(val, source_file);
            unlink("main_fifo");
            return EXIT_SUCCESS;
        case -1:
            perror("reading main fifo");
            exit(EXIT_FAILURE);
        default:
            printf("Wrong size was read\n");
            exit(EXIT_FAILURE);
    }
}

int Read_From_Fifo() {
    Gde;
    int fd_main_fifo;
    if ((fd_main_fifo = open("main_fifo", O_NONBLOCK | O_RDWR)) == -1) {
        if (errno != ENOENT) {
            unlink("main_fifo");
        }

        mkfifo("main_fifo", 0666);
        fd_main_fifo = open("main_fifo", O_NONBLOCK | O_RDWR);
    }

    char name[NAME_SIZE] ="";
    int val = Define_Fifo_Name(name);
    printf("name %s\n", name);


    if (mkfifo(name, 0666) == -1){
        perror("main_fifo");
        exit(EXIT_FAILURE);
    }

    int fd_to_read_from = open(name, O_NONBLOCK | O_RDONLY);
    if (fd_to_read_from == -1){
        printf("name to open %s\n", name);
        perror("f");
        exit(EXIT_FAILURE);
    }

    char buf[PAGE_SIZE] = "";
    /*critical region starts*/
    long have_written = write(fd_main_fifo, &val, sizeof(val));
    /*critical region ends*/
    printf("have written name %d to main fifo %ld\n", val, have_written);

    sleep(5);

    fcntl(fd_to_read_from, F_SETFL, O_RDONLY);
    while (1) {
        long have_read = read(fd_to_read_from, buf, PAGE_SIZE);
        switch (have_read) {
            case 0:
                if (errno == EAGAIN){
                    printf("Writer has closed\n");
                    exit(EXIT_FAILURE);
                }
                printf("Vse zbs\n");
                exit(EXIT_SUCCESS);
            case -1:
                perror("Unexpected error");
                exit(EXIT_FAILURE);
            default:
                break;
                //Vse maybe zbs
        }

        printf("have read %ld, from %d\n", have_read, fd_to_read_from);
        printf("buf %s", buf);
        fflush(NULL);
        if (have_read < PAGE_SIZE)
            return EXIT_SUCCESS;
    }
}

int Reader_Exists(int name, char * source_file) {
	/*critical region continues*/
    Gde;
    printf("name %d\n", name);
    char sname[NAME_SIZE] = "";
    sprintf(sname, "f%d", name);

    int fd_to_write = open(sname, O_NONBLOCK | O_WRONLY);
    if (fd_to_write == -1){
        perror("f");
        exit(EXIT_FAILURE);
    }

    char buf[PAGE_SIZE] = "";
    int fd_source;

    if ((fd_source = open(source_file, O_RDONLY)) == -1) {
        perror("source file");
        exit(EXIT_FAILURE);
    }

    /*critical region ends*/

    fcntl(fd_to_write, F_SETFL, O_WRONLY);

    while (1) {
        ssize_t have_read;
        have_read = read(fd_source, buf, PAGE_SIZE);
        printf("have read %ld ", have_read);
        if (have_read < 0) {
            printf("PIZDA\n");
            exit(EXIT_FAILURE);
        }
        long have_written = write(fd_to_write, buf, (size_t) have_read);
        printf("have written %ld to %d\n", have_written, fd_to_write);
        if (have_read != have_written) {
            printf("errno %d\n", errno);
            perror("wtf");
            exit(EXIT_FAILURE);
        }
        if (have_written < PAGE_SIZE)
            break;
    }


    unlink(sname);

    return EXIT_SUCCESS;
}

int Define_Fifo_Name(char *name) {
    pid_t pid = getpid();
    sprintf(name, "f%d", pid);
    return pid;
}

