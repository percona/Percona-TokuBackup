/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id: test6317.cc 54729 2013-03-26 16:24:56Z bkuszmaul $"

#include <assert.h>
#include <errno.h>
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
    if (0) fprintf(stderr, "backup progress %.2f%%: %s\n", progress*100, progress_string);
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

    int fd;
    {
        char *src = get_src();
        size_t len = strlen(src)+100;
        char name[len];
        int r = snprintf(name, len, "%s/data", src);
        assert((size_t)r<len);
        free(src);
        fd = open(name, O_WRONLY | O_CREAT, 0777);
        assert(fd>=0);
    }
    const size_t NBLOCKS = 1024;
    const size_t bufsize = 1024;
    const size_t max_expected_size = (1+NBLOCKS)*bufsize;

    char buf[bufsize];
    for (size_t i=0; i<bufsize; i++) {
        buf[i]=i%256;
    }

    for (size_t i=0; i<1; i++) {
        ssize_t l = write(fd, buf, bufsize);
        assert(l==(ssize_t)bufsize);
    }

    pthread_t thread;
    start_backup_thread_with_funs(&thread, get_src(), get_dst(), simple_poll, NULL, simple_error, NULL, 0);
    {
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
        size_t len = strlen(dst)+100;
        char name[len];
        {
            int r = snprintf(name, len, "%s/data", dst);
            assert((size_t)r<len);
        }
        free(dst);
        {
            struct stat sbuf;
            int r = stat(name, &sbuf);
            if (r!=0) {
                int er = errno;
                fprintf(stderr, "error doing stat(\"%s\",...)  errno=%d (%s)\n", name, er, strerror(er));
            }
            assert(r==0);

            if (sbuf.st_size > (ssize_t)max_expected_size) {
                printf("size of dest = %ld, but should have been at most %ld\n", sbuf.st_size, max_expected_size);
                abort();
            }
        }
    }
    return 0;
}

