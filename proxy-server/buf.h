//
// Created by fedya on 24.11.17.
//

#ifndef PROXY_SERVER_BUF_H
#define PROXY_SERVER_BUF_H

typedef struct buf {
    char *data;
    char *read_ptr;
    char *write_ptr;
    int pipenum;
    size_t max_size;
    size_t count;
    ssize_t have_read;
} SuperBuf;

SuperBuf *GetBuf(size_t size);

int BufIsFull(SuperBuf *buf);

int BufIsEmpty(SuperBuf *buf);

char *WriteToBuf(SuperBuf *buf);

char *ReadFromBuf(SuperBuf *buf);

void BufDump(SuperBuf *buf, char *destination);

SuperBuf *GetBuf(size_t size) {
    if (size <= 0) {
        fprintf(stderr, "buf size < 0\n");
        exit(EXIT_FAILURE);
    }
    SuperBuf *buf = (SuperBuf *) calloc(1, sizeof(*buf));
    if (!buf) {
        perror("calloc");
        exit(EXIT_FAILURE);
    }
    buf->max_size = size / PAGE_SIZE;
    buf->data = (char *) calloc(size, sizeof(*buf->data));
    if (!buf->data) {
        perror("calloc");
        exit(EXIT_FAILURE);
    }
    buf->read_ptr = buf->data;
    buf->write_ptr = buf->data;
    buf->have_read = PAGE_SIZE;
    return buf;
}

int BufIsFull(SuperBuf *buf) {
    return buf->write_ptr == buf->max_size * PAGE_SIZE + buf->data;
}

int TheLastFromBuf(SuperBuf *buf) {
    return buf->count == 1;
}

int BufIsEmpty(SuperBuf *buf) {
    return !buf->count;
}

char *WriteToBuf(SuperBuf *buf) {
    if (BufIsFull(buf)) {
        fprintf(stderr, "Buf is full, we can't write\n");
        exit(EXIT_FAILURE);
    }
    char *val = buf->write_ptr;
    buf->write_ptr += PAGE_SIZE;
    buf->count++;
    return val;
}

char *ReadFromBuf(SuperBuf *buf) {
    if (BufIsEmpty(buf)) {
        fprintf(stderr, "Buf is empty, we can't read\n");
        exit(EXIT_FAILURE);
    }
    char *val = buf->read_ptr;
    buf->read_ptr += PAGE_SIZE;
    buf->count--;
    if (!buf->count) {
        buf->write_ptr = buf->data;
        buf->read_ptr = buf->data;
    }
    return val;
}

void BufDump(SuperBuf *buf, char *destination) {
    sprintf(destination, "data %p\n", buf->data);
    strncpy(destination + strlen(destination), buf->data, 20);
    sprintf(destination + strlen(destination), "\nread ptr %ld, write ptr %ld\npipenum %d, max size %ld\n",
            ((long unsigned) buf->read_ptr - (long unsigned) buf->data) / PAGE_SIZE,
            ((long unsigned) buf->write_ptr - (long unsigned) buf->data) / PAGE_SIZE, buf->pipenum, buf->max_size);
}


#endif //PROXY_SERVER_BUF_H
