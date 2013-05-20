#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pthread.h>

#include "backup_test_helpers.h"
#include "backup_internal.h"

#define FIRSTBYTES "first bytes"
#define MOREBYTES  "more bytes"

static char *src, *dst;

static void *doit(void* whoami_v) {
    int whoami = *(int*)whoami_v;
    int fd = openf(O_CREAT | O_WRONLY, S_IRWXU | S_IRWXG | S_IRWXO, "%s/tfile%d", src, whoami);
    check(fd>=0);
    {
        ssize_t r = write(fd, FIRSTBYTES, sizeof(FIRSTBYTES));
        check(r==sizeof(FIRSTBYTES));
    }
    usleep(1);
    {
        int r = close(fd);
        check(r==0);
    }
    return whoami_v;
}

static void multithreaded_work(void) {
    int fd0 = openf(O_CREAT | O_WRONLY, S_IRWXU | S_IRWXG | S_IRWXO,  "%s/file0", src);
    check(fd0>=0);

    int fd1 = openf(O_CREAT | O_WRONLY, S_IRWXU | S_IRWXG | S_IRWXO,  "%s/filed1", src);

    const int N_THREADS = 2;
    pthread_t threads[N_THREADS];
    int       whoami[N_THREADS];
    for (int i=0; i<N_THREADS; i++) {
        whoami[i] = i;
    }
    if (0) {
        doit(&whoami[0]);
    } else {
        for (int i=0; i<N_THREADS; i++) {
            whoami[i] = i;
            int r = pthread_create(&threads[i], NULL, doit, &whoami[i]);
            check(r==0);
        }
        for (int i=0; i<N_THREADS; i++) {
            void *whowasi;
            int r = pthread_join(threads[i], &whowasi);
            check(r==0);
            check((int*)whowasi==&whoami[i]);
        }
    }
    {
        ssize_t r = write(fd0, FIRSTBYTES, sizeof(FIRSTBYTES));
        check(r==sizeof(FIRSTBYTES));
    }

    {
        ssize_t r = write(fd1, MOREBYTES, sizeof(MOREBYTES));
        check(r==sizeof(MOREBYTES));
    }

    { int r = close(fd0); check(r==0); }
    { int r = close(fd1); check(r==0); }
}

int test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    setup_destination();
    setup_source();
    src = get_src();
    dst = get_dst();

    backup_set_keep_capturing(true);
    pthread_t thread;
    start_backup_thread(&thread);
    multithreaded_work();
    backup_set_keep_capturing(false);
    finish_backup_thread(thread);
    {
        int status = systemf("diff -r %s %s", src, dst);
        check(status!=-1);
        check(WIFEXITED(status));
        check(WEXITSTATUS(status)==0);
    }
    free(src);
    free(dst);
    return 0;
}
