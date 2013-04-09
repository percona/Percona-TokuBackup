/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id: open_write_close.cc 54729 2013-03-26 16:24:56Z bkuszmaul $"

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

enum open_errors {
    NO_ACCESS             = EACCES,
    ALREADY_EXISTS        = EEXIST,
    OUTSIDE_SPACE         = EFAULT,
    DIR_WRITE_ACCESS      = EISDIR,
    LOOP                  = ELOOP, // Too many symbolic links.
    MAX_PROC_FILES        = EMFILE,
    TOO_LONG              = ENAMETOOLONG,
    MAX_SYS_FILES         = ENFILE,
    NO_DEVICE             = ENODEV,
    NO_ENTRY              = ENOENT,
    NO_MEMORY             = ENOMEM,
    NO_SPACE              = ENOSPC,
    NOT_A_DIR             = ENOTDIR,
    WTF                   = ENXIO,
    TOO_LARGE             = EOVERFLOW,
    NO_PERMISSION         = EPERM,
    READ_ONLY_FILE        = EROFS,
    FILE_IS_EXECUTING     = ETXTBSY,
    CONTENTION            = EWOULDBLOCK
};

static volatile int iteration = 0;

static open_fun_t original_open;

static int test_create(const char *file, int oflag, mode_t mode)
{
    return original_open(file, oflag, mode);
}

static int test_open(const char *file, int oflag)
{
    int result = 0;
    int it = __sync_fetch_and_add(&iteration, 1);
    switch (it) {
    case 2:
        errno = ENOSPC;
        result = -1;
        break;
    default:
        result = original_open(file, oflag);
        break;
    }

    printf("file: %s, current open iteration = %d\n", file, it);
    return result;
}

static int my_open(const char *file, int oflag, ...)
{
    int result = 0;
    if (oflag & O_CREAT) {
        va_list ap;
        va_start(ap, oflag);
        mode_t mode = va_arg(ap, mode_t);
        va_end(ap);
        result = test_create(file, oflag, mode);
    } else {
        result = test_open(file, oflag);
    }

    return result;
}

static int expect_error = 0;
static void my_error_fun(int e, const char *s, void *ignore) {
    assert(ignore==NULL);
    fprintf(stderr, "Got error %d (I expected errno=%d) (%s)\n", e, expect_error, s);
}

static int test_open_failures(void)
{
    int result = 0;
    // 1.  Create the file.
    const char * src = get_src();
    int fd = openf(O_RDWR | O_CREAT, 0777, "%s/my.data", src);
    assert(fd >= 0);
    const int SIZE = 10;
    char buf[SIZE] = {0};
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
                                  ENOSPC);

    while (!backup_is_capturing()) { sched_yield(); }
    sleep(2);

    // 4.  Try to open the created file (to get a different fd).
    expect_error = ENOSPC;
    int fd2 = openf(O_RDWR, 0777, "%s/my.data", src);
    assert(fd2 >= 0);

    // 5.  Verify that error was returned.
    

    // 6. Cleanup
    backup_set_keep_capturing(false);
    finish_backup_thread(backup_thread);
    free((void*) src);

    return result;
}

int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    int result = 0;
    setup_source();
    setup_destination();
    original_open = register_open(my_open);
    result = test_open_failures();
    cleanup_dirs();
    return result;
}
