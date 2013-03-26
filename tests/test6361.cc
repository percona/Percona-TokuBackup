/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id: test6317.cc 54729 2013-03-26 16:24:56Z bkuszmaul $"

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "backup.h"
#include "backup_test_helpers.h"

static int simple_poll(float progress, const char *progress_string, void *poll_extra) {
    assert(poll_extra==NULL);
    fprintf(stderr, "backup progress %.2f%%: %s\n", progress*100, progress_string);
    return 0;
}
static void simple_error(int errnum, const char *error_string, void *error_extra) {
    assert(error_extra==NULL);
    fprintf(stderr, "backup error: errnum=%d (%s)\n", errnum, error_string);
    abort();
}

int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    cleanup_dirs(); // remove destination dir
    setup_source();
    setup_destination();
    pthread_t thread;
    start_backup_thread_with_funs(&thread, get_src(), get_dst(), simple_poll, NULL, simple_error, NULL, 0);
    const size_t NBLOCKS = 1024;
    const size_t bufsize = 1024;
    const size_t max_size = NBLOCKS*bufsize;
    {
        int fd;
        {
            char *src = get_src();
            char len = strlen(src)+100;
            char name[len];
            int r = snprintf(name, len, "%sdata", src);
            assert(r<len);
            free(src);
            printf("opened %s\n", name);
            fd = open(name, O_WRONLY | O_CREAT, 0777);
            assert(fd>=0);
        }
        char buf[bufsize];
        for (size_t i=0; i<bufsize; i++) {
            buf[i]=i%256;
        }
        for (size_t i=0; i<NBLOCKS; i++) {
            ssize_t l = write(fd, buf, bufsize);
            assert(l==(ssize_t)bufsize);
        }
        finish_backup_thread(thread);
        for (size_t i=0; i<NBLOCKS; i++) {
            ssize_t l = write(fd, buf, bufsize);
            assert(l==(ssize_t)bufsize);
        }
        {
            int r = close(fd);
            assert(r==0);
        }
    }
    {
        char *dst = get_dst();
        char len = strlen(dst)+100;
        char name[len];
        {
            int r = snprintf(name, len, "%sdata", dst);
            assert(r<len);
        }
        free(dst);
        {
            struct stat sbuf;
            int r = stat(name, &sbuf);
            assert(r==0);

            if (sbuf.st_size > (ssize_t)max_size) {
                printf("size of dest = %ld, but should have been at most %ld\n", sbuf.st_size, max_size);
                abort();
            }
        }
    }
    return 0;
}

