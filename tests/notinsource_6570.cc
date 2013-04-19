#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id: unlink_during_copy_test6515.cc 55458 2013-04-13 21:44:50Z bkuszmaul $"

// Test doing operations that are in a directory that's not backed up.

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "backup_test_helpers.h"

char *not_src;

void exercise(void) {
    int nlen = strlen(not_src)+10;
    char filea_name[nlen];
    snprintf(filea_name, nlen, "%s/filea", not_src);
    int fda = open(filea_name, O_RDWR|O_CREAT, 0777);
    assert(fda>=0);
    {
        int r = close(fda);
        assert(r==0);
    }
    {
        int r = unlink(filea_name);
        assert(r==0);
    }
}

int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    char *src = get_src();
    size_t slen = strlen(src);
    const char N_EXTRA_BYTES = 10;
    not_src = (char*)malloc(slen+N_EXTRA_BYTES);
    strncpy(not_src, src, slen+N_EXTRA_BYTES);
    const char go_back_n_bytes = 7;
    printf("backed up =%s\n",not_src+slen-go_back_n_bytes);
    assert(0==strcmp(not_src+slen-go_back_n_bytes, ".source"));
    size_t n_written = snprintf(not_src+slen - go_back_n_bytes, N_EXTRA_BYTES+go_back_n_bytes, ".notsource");
    assert(n_written< N_EXTRA_BYTES+go_back_n_bytes);
    {
        int r = systemf("rm -rf %s", not_src);
        assert(r==0);
    }
    {
        int r = mkdir(not_src, 0777);
        assert(r==0);
    }
    setup_source();
    setup_destination();

    int fd = openf(O_WRONLY|O_CREAT, 0777, "%s/data", src);
    assert(fd>=0);
    {
        const int bufsize=1024;
        const int nbufs  =1024;
        char buf[bufsize];
        for (int i=0; i<bufsize; i++) {
            buf[i]=i%256;
        }
        for (int i=0; i<nbufs; i++) {
            ssize_t r = write(fd, buf, bufsize);
            assert(r==bufsize);
        }
    }
    tokubackup_throttle_backup(1L<<18); // quarter megabyte per second, so that's 4 seconds.
    pthread_t thread;
    start_backup_thread(&thread);
    while (!backup_thread_is_done()) {
        exercise();
    }
    exercise();
    finish_backup_thread(thread);
    // And do two more.
    exercise();
    exercise();
    free(src);
    return 0;
}
