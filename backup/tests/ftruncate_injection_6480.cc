/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
/*======
This file is part of Percona TokuBackup.

Copyright (c) 2006, 2015, Percona and/or its affiliates. All rights reserved.

    Percona TokuBackup is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License, version 2,
    as published by the Free Software Foundation.

     Percona TokuBackup is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Percona TokuBackup.  If not, see <http://www.gnu.org/licenses/>.

----------------------------------------

    Percona TokuBackup is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License, version 3,
    as published by the Free Software Foundation.

    Percona TokuBackup is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with Percona TokuBackup.  If not, see <http://www.gnu.org/licenses/>.
======= */

#ident "$Id$"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdarg.h>
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
    check(ignore==NULL);
    fprintf(stderr, "Got error %d (I expected errno=%d) (%s)\n", e, expect_error, s);
    error_result = e;
}

static int test_ftruncate_failures(void)
{
    int result = 0;
    // 1.  Create the file.
    const char * src = get_src();
    int fd = openf(O_RDWR | O_CREAT, 0777, "%s/my.data", src);
    check(fd >= 0);
    const int SIZE = 10;
    char buf[SIZE] = {0};
    // write to the file.
    int write_r = write(fd, buf, SIZE);
    check(write_r == SIZE);
    int close_r = close(fd);
    check(close_r == 0);

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
    check(fd2 >= 0);
    const off_t TRUNCATE_AMOUNT = SIZE - 1;
    int ftruncate_r = ftruncate(fd2, TRUNCATE_AMOUNT);
    check(ftruncate_r == 0);

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
