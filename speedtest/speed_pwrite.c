/* A speedtest using multithreaded pwrite as the inner loop. */

/* Link with, and without the backuplib, and compare performance */
#define _FILE_OFFSET_BITS 64 
#define _LARGEFILE64_SOURCE
#define _GNU_SOURCE
#include <assert.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

int fd;
int n_threads = 16;
int n_writes_per_thread=5000;
const int blocksize = 4096;

static void* runwrites(void *whoamip) {
    long whoami = *(long*)whoamip;
    char *buf;
    {
        void *vbuf;
        int r = posix_memalign(&vbuf, blocksize, blocksize);
        assert(r==0);
        buf = (char*)vbuf;
    }
    for (int i=0; i<blocksize; i++) {
        buf[i]=random();
    }
    //printf("whoami=%ld\n", whoami);
    for (int i=0; i<n_writes_per_thread; i++) {
        long ra = random()%n_writes_per_thread;
        long blocknum = (ra*n_threads)+whoami; // different threads operate on different stripes.
        long offset = blocknum*blocksize;
        buf[ra%blocksize]++; // make the blocks a little different
        //printf("ra=%ld blocknum=%ld offset=%ld\n", ra, blocknum, offset);
        ssize_t wr = pwrite(fd, buf, blocksize, offset);
        if (wr<0) perror("Trying to write");
        assert(wr==blocksize);
    }
    return whoamip;
}

int main (int argc __attribute__((unused)), char *argv[]  __attribute__((unused))) {
    fd = open("speedtest.data", O_RDWR|O_CREAT|O_DIRECT, 0777);
    assert(fd>=0);
    pthread_t threads[n_threads];
    long      whoami[n_threads];
    for (int i=0; i<n_threads; i++) {
        whoami[i]=i;
        int r = pthread_create(&threads[i], NULL, runwrites, &whoami[i]);
        assert(r==0);
    }
    for (int i=0; i<n_threads; i++) {
        void *v;
        int r = pthread_join(threads[i], &v);
        assert(r==0);
        assert((long*)v == &whoami[i]);
    }
    return 0;
}
