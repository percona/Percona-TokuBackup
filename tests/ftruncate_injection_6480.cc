/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id: ftruncate_injection_6470.cc 54729 2013-03-26 16:24:56Z bkuszmaul $"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <assert.h>
#include <malloc.h>
#include <unistd.h>

#include "backup.h"
#include "backup_internal.h"
#include "backup_test_helpers.h"
#include "real_syscalls.h"

static const int ERRORS_TO_CHECK = 1;

static int iteration = 0;

const int ERROR = EIO;

static ftruncate_fun_t original_ftruncate;

static int test_ftruncate(int fildes, off_t length)
{
    int it = __sync_fetch_and_add(&iteration, 1);
    int result = 0;
    switch (it) {
    case 1:
        errno = ERROR;
        result = -1;
        break;
    default:
        result = original_ftruncate(fildes, length);
        break;
    }

    printf("fd: %d, current lseek iteration = %d\n", fildes, it); 
    return result;
}

static int my_ftruncate(int fildes, off_t length)
{
    int result = 0;
    result = test_ftruncate(fildes, length);
    return result;
}

static int expect_error = 0;
static int error_result = 0;
static void my_error_fun(int e, const char *s, void *ignore) {
    assert(ignore==NULL);
    fprintf(stderr, "Got error %d (I expected errno=%d) (%s)\n", e, expect_error, s);
    error_result = e;
}

static int test_ftruncate_failures(void)
{
    int result = 0;
    // 1.  Create the file.
    const char * src = get_src();
    int fd = openf(O_RDWR | O_CREAT, 0777, "%s/my.data", src);
    assert(fd >= 0);
    const int SIZE = 10;
    char buf[SIZE] = {0};
    // write to the file.
    int write_r = write(fd, buf, SIZE);
    assert(write_r == SIZE);
    int close_r = close(fd);
    assert(close_r == 0);

    // 2.  Set backup to pause.
    backup_set_keep_capturing(true);

    // 3.  Start backup.
    pthread_t backup_thread;
    start_backup_thread_with_funs(&backup_thread,
                                  get_src(),
                                  get_dst(),
                                  simple_poll_fun,
                                  NULL,
                                  my_error_fun,
                                  NULL,
                                  ERROR);

    while (!backup_is_capturing()) { sched_yield(); }
    sleep(1);

    // 4.  Open a new fd and truncate (SIZE - 1).
    expect_error = ERROR;
    int fd2 = openf(O_RDWR, 0777, "%s/my.data", src);
    assert(fd2 >= 0);
    const off_t TRUNCATE_AMOUNT = SIZE - 1;
    int ftruncate_r = ftruncate(fd2, TRUNCATE_AMOUNT);
    assert(ftruncate_r == 0);

    // 5.  Verify that error was returned.
    backup_set_keep_capturing(false);
    finish_backup_thread(backup_thread);
    if (error_result == ERROR) {
        pass();
    } else {
        result = -1;
        fail();
    }

    printf("error_result = %d\n", error_result);

    // 6. Cleanup
    free((void*) src);

    return result;
}

int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    int result = 0;
    setup_source();
    setup_destination();
    original_ftruncate = register_ftruncate(my_ftruncate);
    result = test_ftruncate_failures();
    cleanup_dirs();
    return result;
}
