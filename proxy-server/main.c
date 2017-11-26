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
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include <c_functions.h>

#define NDEBUG 10

#include <my_debug.h>

struct __pipe {
    int child_will_read;
    int child_will_write;
};

typedef struct pipe {
    struct __pipe read;
    struct __pipe write;
    long num;
} Pipe;



struct timeval timeout = {100, 0};

static int MAXFD = 0;
static const size_t CHILD_BUF_SIZE = (128 * 1024);
static const size_t PAGE_SIZE = (512);
static int fd_to_read_from = 0;
static long n = 0;

static int Child(Pipe);

int ClosePipes(Pipe *pipes, long zbs, long n) ;

#include "buf.h"
#include "c_list.h"

#define GetPipeNum(elem) (((SuperBuf *) ((elem)->data))->pipenum)

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Wrong argument count\n");
        exit(EXIT_FAILURE);
    }

    n = my_strtol(argv[1]);
    if (n <= 1)
        exit(EXIT_FAILURE);

    Pipe *pipes = (Pipe *) calloc((size_t) n, sizeof(Pipe));
    for (long i = 0; i < n; i++) {
        if (pipe((int *) &pipes[i].read)) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
        if (pipe((int *) &pipes[i].write)) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
        //printf("pipe %ld read->read %d, read->write %d, write->read %d, write->write %d\n", i, pipes[i].read.child_will_read, pipes[i].read.child_will_write, pipes[i].read.child_will_write, pipes[i].write.child_will_write);
        pipes[i].num = i;
    }
    //MAXFD = pipes[n - 1].write + 1;
    MAXFD = 1024;

    fd_to_read_from = open(argv[2], O_RDONLY);
    if (fd_to_read_from == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    dfprintf(stderr, "File fd %d\n", fd_to_read_from);
    for (long i = 0; i < n; i++) {
        pid_t pid = fork();
        if (!pid) {
            ClosePipes(pipes, i, n);
            return Child(pipes[i]);
        }
    }

    for (long i = 0; i < n; i++) {
        close(pipes[i].write.child_will_write);
        close(pipes[i].read.child_will_read);
    }

    SuperBuf **bufs = calloc((size_t) n - 1, sizeof(SuperBuf *));
    ListElem *read_elem = NULL;
    ListElem *write_elem = NULL;
    for (int i = 0; i < n - 1; i++) {
        size_t size = 0;
        if (i < 7)
            size = (size_t) pow(3, i) * 512;
        else
            size = 1 << 20;
        bufs[i] = GetBuf(size);
        read_elem = AddAfter(read_elem, bufs[i]);
        write_elem = AddAfter(write_elem, bufs[i]);
        (*bufs[i]).pipenum = i;
    }
    List *read_list = read_elem->header;
    List *write_list = write_elem->header;
    sprintf(read_list->name, "read");
    sprintf(write_list->name, "write");

    //ListDump(read_list);
    //ListDump(write_list);
    close(fd_to_read_from);

    for (;!ListIsEmpty(write_list);) {
        fd_set rd, wr;
        FD_ZERO(&rd);
        FD_ZERO(&wr);           //n read ends, n write ends
        for (ListElem *elem = read_list->first; elem; elem = elem->next) {
            if (!BufIsFull(elem->data)) {
                FD_SET(pipes[GetPipeNum(elem)].write.child_will_read, &rd);
                dfprintf(stderr, "\n--set read %d", pipes[GetPipeNum(elem)].write.child_will_read);
            }
        }

        //printf("Write list count %ld\n", write_list->count);
        for (ListElem *elem = write_list->first; elem; elem = elem->next) {
            if (!BufIsEmpty(elem->data)) {
                FD_SET(pipes[GetPipeNum(elem) + 1].read.child_will_write, &wr);
                dfprintf(stderr, "\n--set write %d", pipes[GetPipeNum(elem) + 1].read.child_will_write);
            }
        }
        int nfds = select(MAXFD, &rd, &wr, NULL, &timeout);
        if (nfds == -1) {
            perror("select");
            exit(EXIT_FAILURE);
        }
        if (!nfds) {
            fprintf(stderr, "No available fds\n");
            exit(EXIT_FAILURE);
        }
        for (ListElem *elem = read_list->first; elem;) {
            Gde;
            ListElem *next = elem->next;
            if (FD_ISSET(pipes[GetPipeNum(elem)].write.child_will_read, &rd)) {
                ssize_t have_read = read(pipes[GetPipeNum(elem)].write.child_will_read, WriteToBuf(elem->data), PAGE_SIZE);
                dfprintf(stderr, "\n--have read %ld from %d", have_read, pipes[GetPipeNum(elem)].write.child_will_read);
                fflush(NULL);
                if (have_read == -1) {
                    dfprintf(stderr, "\n--error on fd %d", pipes[GetPipeNum(elem)].write.child_will_read);
                    perror("\nread");
                    exit(EXIT_FAILURE);
                }
                if (!have_read) {
                    fprintf(stderr, "\nEbani nasos read\n");
                    exit(EXIT_FAILURE);
                }
                if (have_read < PAGE_SIZE) {
                    ((SuperBuf *) elem->data)->have_read = have_read;
                    DeleteElem(elem);
                }
            }
            elem = next;
        }

        for (ListElem *elem = write_list->first; elem;) {
            ListElem *next = elem->next;
            if (FD_ISSET(pipes[GetPipeNum(elem) + 1].read.child_will_write, &wr)) {
                size_t to_write = PAGE_SIZE;
                if (TheLastFromBuf(elem->data))
                    to_write = (size_t) ((SuperBuf *) elem->data)->have_read;
                ssize_t have_written = write(pipes[GetPipeNum(elem) + 1].read.child_will_write, ReadFromBuf(elem->data), to_write);
                dfprintf(stderr, "\n--have written %ld to %d", have_written, pipes[GetPipeNum(elem) + 1].read.child_will_write);
                fflush(NULL);
                if (have_written == -1) {
                    dfprintf(stderr, "\n--error on fd %d", pipes[GetPipeNum(elem)].read.child_will_write);
                    perror("\nwrite");
                    exit(EXIT_FAILURE);
                }
                if (!have_written) {
                    dfprintf(stderr, "\nEbani nasos write\n");
                    exit(EXIT_FAILURE);
                }

                if (have_written < PAGE_SIZE)
                    DeleteElem(elem);
            }
            elem = next;
        }
    }
    for (int i = 0; i < n - 1; i++) {
        free(bufs[i]->data);
    }
    free(bufs);
    free(pipes);
    ListDtor(write_list);
    ListDtor(read_list);
    return EXIT_SUCCESS;
}

int ClosePipes(Pipe *pipes, long zbs, long n) {
    for (long i = 0; i < n; i++) {
        if (i != zbs) {
            close(pipes[i].write.child_will_write);
            close(pipes[i].read.child_will_read);
        }
        close(pipes[i].write.child_will_read);
        close(pipes[i].read.child_will_write);
    }
}

static int Child(Pipe pipe) {
    if (pipe.num == n - 1) {
        close(pipe.write.child_will_write);
        pipe.write.child_will_write = STDOUT_FILENO;
    }
    if (pipe.num == 0) {
        close(pipe.read.child_will_read);
        pipe.read.child_will_read = fd_to_read_from;
    }

    dfprintf(stderr, "\n\tI'm %ld read %d write %d", pipe.num, pipe.read.child_will_read, pipe.write.child_will_write);
    SuperBuf *buf = GetBuf(CHILD_BUF_SIZE);
    int stop = 0;
    for (;;) {
        fd_set rd, wr;
        FD_ZERO(&rd);
        FD_ZERO(&wr);
        if (!stop && !BufIsFull(buf)) {                          //We can write to the buf so we need to read from fd
            FD_SET(pipe.read.child_will_read, &rd);
        }
        if (!BufIsEmpty(buf)) {                                  //Vise versa
            FD_SET(pipe.write.child_will_write, &wr);
        }
        int nfds = select(MAXFD, &rd, &wr, NULL, &timeout);
        if (nfds == -1) {
            perror("select");
            exit(EXIT_FAILURE);
        }
        if (!nfds) {
            fprintf(stderr, "No available fds\n");
            exit(EXIT_FAILURE);
        }

        if (!stop && FD_ISSET(pipe.read.child_will_read, &rd)) {
            ssize_t have_read = read(pipe.read.child_will_read, WriteToBuf(buf), PAGE_SIZE);
            dfprintf(stderr, "\nhave read %ld from %d i'm %ld", have_read, pipe.read.child_will_read, pipe.num);
            if (have_read == -1) {
                dfprintf(stderr, "\nerror on fd %d", pipe.read.child_will_read);
                perror("\nread");
                exit(EXIT_FAILURE);
            }
            if (!have_read) {
                fprintf(stderr, "\nPIZDA\n");
                exit(EXIT_FAILURE);
            }
            if (have_read < PAGE_SIZE) {
                buf->have_read = have_read;
                stop = 1;
            }
        }
        if (FD_ISSET(pipe.write.child_will_write, &wr)) {
            size_t to_write = PAGE_SIZE;
            if (TheLastFromBuf(buf))
                to_write = (size_t) (buf)->have_read;
            ssize_t have_written = write(pipe.write.child_will_write, ReadFromBuf(buf), to_write);
            dfprintf(stderr, "\nhave written %ld to %d i'm %ld", have_written, pipe.write.child_will_write, pipe.num);
            if (have_written == -1) {
                fprintf(stderr, "\nerror on fd %d nwrite = %ld num = %ld", pipe.read.child_will_read, to_write, pipe.num);
                perror("\nwrite");
                exit(EXIT_FAILURE);
            }
            if (have_written < PAGE_SIZE)
                break;
        }
    }
    free(buf->data);
    free(buf);
    return EXIT_SUCCESS;
}
