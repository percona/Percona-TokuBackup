#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "backup_test_helpers.h"

int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    char *src = get_src();
    char *dst = get_dst();
    setup_source();
    setup_destination();
    setup_dirs();
    pthread_t thread;
    const int N = 2;
    char fname[N+1][1000];
    int fds[N+1];
    unsigned char buf[1024];
    for (size_t i=0; i<sizeof(buf); i++) buf[i]=i%256;
    for (int i=0; i<N+1; i++) {
        {
            int r = snprintf(fname[i], sizeof(fname[i]), "%s/f%d", src, i);
            printf("r=%d fname[%d]=%s\n", r, i, fname[i]);
            check(r<(int)sizeof(fname[i]));
        }
        fds[i] = open(fname[i], O_WRONLY|O_CREAT, 0777);
        check(fds[i]>=0);
        for (int j=0; j<1024; j++) {
            ssize_t r = write(fds[i], buf, sizeof(buf));
            check(r==sizeof(buf));
        }
    }
    tokubackup_throttle_backup(1L<<18); // 1L<<18 is half a mebibyte per second, so that's 4 seconds.
    start_backup_thread(&thread);
    usleep(100000);
    for (int i=0; i<N; i++) {
        int r = unlink(fname[i]);
        check(r==0);
    }
    finish_backup_thread(thread);
    for (int i=0; i<N+1; i++) {
        int r = close(fds[i]);
        check(r==0);
    }
    {
        int status = systemf("diff -r %s %s", src, dst);
        check(status!=-1);
        check(WIFEXITED(status));
        printf("status=%d\n", status);
        check(WEXITSTATUS(status)==0);
    }
    free(src);
    free(dst);
    return 0;
}
