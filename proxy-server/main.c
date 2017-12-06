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

typedef struct pipe {
    int fd0[2];
    int fd1[2];
    long num;
} Pipe;

static int MAXFD = 0;
static const size_t CHILD_BUF_SIZE = (128 * 1024);
static const size_t PAGE_SIZE = (512);
static int fd_to_read_from = 0;
static long n = 0;
static const int LUNEV_BIG_NUMBER = 1 << 20;
static const int SOMETHING_THAT_IF_3_IS_NASTILY_POWERED_BY_THIS_AND_THEN_SNIDELY_MULLED_BY_PAGESIZE_IS_GREATER_THAN_LUNEV_BIG_NUMBER = 7;
static int Child(Pipe);

int ClosePipes(Pipe *pipes, long zbs, long n) ;

#include "buf.h"

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
        if (pipe((int *) &pipes[i].fd0)) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
        if (pipe((int *) &pipes[i].fd1)) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
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
        close(pipes[i].fd0[0]);
        close(pipes[i].fd1[1]);
        fcntl(pipes[i].fd0[1], F_SETFL, O_WRONLY | O_NONBLOCK);
        fcntl(pipes[i].fd1[0], F_SETFL, O_RDONLY | O_NONBLOCK);
    }
    close(pipes[0].fd0[1]);
    close(pipes[n - 1].fd1[0]);
    SuperBuf **bufs = calloc((size_t) n - 1, sizeof(SuperBuf *));
    for (long i = n - 2, count = 1; i >= 0; i--, count *= 3) {
        size_t size = 0;
        if (n - i - 2 < SOMETHING_THAT_IF_3_IS_NASTILY_POWERED_BY_THIS_AND_THEN_SNIDELY_MULLED_BY_PAGESIZE_IS_GREATER_THAN_LUNEV_BIG_NUMBER)
            size = (size_t) count * PAGE_SIZE;
        else
            size = LUNEV_BIG_NUMBER;
        bufs[i] = GetBuf(size);
        bufs[i]->pipenum = n - 2 - i;
    }
    close(fd_to_read_from);
    for (;;) {
        fd_set rd, wr;
        FD_ZERO(&rd);
        FD_ZERO(&wr);           //n read ends, n write ends
        long ready = 0;
        for (int j = 0; j < n - 1; j ++) {
            if (pipes[j].fd1[0] != -1 && !BufIsFull(bufs[j])) {
                FD_SET(pipes[j].fd1[0], &rd);
                ready++;
                dfprintf(stderr, "\n--set read %d", pipes[j].fd1[0]);
            }
        }
        for (int j = 0; j < n - 1; j ++) {
            if (pipes[j + 1].fd0[1] != -1 && !BufIsEmpty(bufs[j])) {
                FD_SET(pipes[j + 1].fd0[1], &wr);
                ready++;
                dfprintf(stderr, "\n--set write %d", pipes[j + 1].fd0[1]);
            }
        }
        if (!ready)
            break;
        int nfds = select(MAXFD, &rd, &wr, NULL, NULL);
        if (nfds == -1) {
            perror("select");
            exit(EXIT_FAILURE);
        }
        if (!nfds) {
            fprintf(stderr, "No available fds\n");
            exit(EXIT_FAILURE);
        }
        for (long i = 0; i < n - 1; i++) {
            if (FD_ISSET(pipes[i].fd1[0], &rd)) {
                size_t to_read = PAGE_SIZE;
                ssize_t have_read = read(pipes[i].fd1[0], (bufs[i])->write_ptr, to_read);
                bufs[i]->write_ptr += have_read;
                dfprintf(stderr, "\n--have read %ld from %d", have_read, pipes[i].fd1[0]);
                fflush(NULL);
                if (have_read == -1) {
                    perror("\nread");
                    exit(EXIT_FAILURE);
                }
                if (!have_read || have_read < to_read) {
                    close(pipes[i].fd1[0]);
                    close(pipes[i].fd0[1]);
                    pipes[i].fd1[0] = -1;
                    pipes[i].fd0[1] = -1;
                }
            }
        }

        for (long i = 0; i < n - 1; i++) {
            if (FD_ISSET(pipes[i + 1].fd0[1], &wr)) {
                size_t to_write = bufs[i]->write_ptr - bufs[i]->read_ptr;
                if (to_write >= PAGE_SIZE)
                    to_write = PAGE_SIZE;
                ssize_t have_written = write(pipes[i + 1].fd0[1], bufs[i]->read_ptr, to_write);
                bufs[i]->read_ptr += have_written;
                if (bufs[i]->read_ptr == bufs[i]->write_ptr) {
                    bufs[i]->read_ptr = bufs[i]->data;
                    bufs[i]->write_ptr = bufs[i]->data;
                }
                dfprintf(stderr, "\n--have written %ld to %d", have_written, pipes[i + 1].fd0[1]);
                fflush(NULL);
                if (have_written == -1) {
                    dfprintf(stderr, "\n--error on fd %d", pipes[i + 1].fd0[1]);
                    perror("\nwrite");
                    exit(EXIT_FAILURE);
                }
                if (!have_written) {
                    dfprintf(stderr, "\n((( write\n");
                    exit(EXIT_FAILURE);
                }

            }
        }
    }
    for (int i = 0; i < n - 1; i++) {
        free(bufs[i]->data);
    }
    free(bufs);
    free(pipes);
    return EXIT_SUCCESS;
}

int ClosePipes(Pipe *pipes, long zbs, long n) {
    for (long i = 0; i < n; i++) {
        if (i != zbs) {
            close(pipes[i].fd1[1]);
            close(pipes[i].fd0[0]);
        }
        close(pipes[i].fd0[1]);
        close(pipes[i].fd1[0]);
    }
}

static int Child(Pipe pipe) {
    if (pipe.num == n - 1) {
        close(pipe.fd1[1]);
        pipe.fd1[1] = STDOUT_FILENO;
    }
    if (pipe.num == 0) {
        close(pipe.fd0[0]);
        pipe.fd0[0] = fd_to_read_from;
    }

    SuperBuf *buf = GetBuf(CHILD_BUF_SIZE);

    for(;;) {
        ssize_t have_read = read(pipe.fd0[0], buf->data, CHILD_BUF_SIZE);
        if (have_read == -1) {
            dfprintf(stderr, "\nerror on fd %d", pipe.fd0[0]);
            perror("\nread");
            exit(EXIT_FAILURE);
        }
        if (!have_read) {
            break;
        }
        ssize_t have_written = write(pipe.fd1[1], buf->data, have_read);
        if (have_written == -1) {
            perror("\nwrite");
            exit(EXIT_FAILURE);
        }

        if (have_written != have_read) {
            fprintf(stderr, "ZHOPA\n");
            exit(EXIT_FAILURE);
        }
    }

    free(buf->data);
    free(buf);
    return EXIT_SUCCESS;
}
