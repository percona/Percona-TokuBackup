/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <valgrind/helgrind.h>

#include "backup.h"
#include "backup_test_helpers.h"

volatile int ftruncate_done = 0;

static int ftruncate_poll(float progress, const char *progress_string, void *extra) {
    assert(0<=progress && progress<1);
    assert(extra==NULL);
    assert(strlen(progress_string)>8);
    while (!ftruncate_done) {
        sched_yield();
    }
    return 0;
}
    
//
void test_truncate(void) {
    VALGRIND_HG_DISABLE_CHECKING(&ftruncate_done, sizeof(ftruncate_done));

    int result = 0;
    setup_source();
    setup_dirs();
    setup_destination();

    char *src = get_src();

    pthread_t thread;
    start_backup_thread_with_funs(&thread, get_src(), get_dst(),
                                  ftruncate_poll, NULL,
                                  dummy_error, NULL,
                                  0);

    // Create a new file.
    int fd = openf(O_CREAT | O_RDWR, 0777, "%s/my.data", src);
    assert(fd >= 0);
    free(src);

    // Write a large chunk of data to it.
    result = write(fd, "Hello World", 12);

    // Truncate the end off.
    {
        int r = ftruncate(fd, 6);
        assert(r==0);
    }
    ftruncate_done  = 1;
    const int SIZE = 20;
    char buf_source[SIZE];
    size_t src_n_read = pread(fd, buf_source, SIZE, 0);
    assert(src_n_read==6);

    finish_backup_thread(thread);

    // Confirm that the both files are the same size.
    char *dst = get_dst();
    int backup_fd = openf(O_RDWR, 0, "%s/my.data", dst);
    assert(backup_fd>=0);
    free(dst);
    char buf_copy[SIZE];
    size_t dst_n_read = read(backup_fd, buf_copy, SIZE);
    assert(dst_n_read==6);
    result = memcmp(buf_source, buf_copy, src_n_read);

    if (result != 0) { 
        fail();
        printf(": BACKUP: <TEST> Files don't match! %s != %s\n", buf_source, buf_copy);
        fail();
    } else {
        pass();
    }

    printf(": test_truncate()\n");
}

int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    test_truncate();
    return 0;
}
