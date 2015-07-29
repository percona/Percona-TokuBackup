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

static volatile int iteration = 0;

static unlink_fun_t original_unlink;

static int test_unlink(const char *file)
{
    int result = 0;
    int it = __sync_fetch_and_add(&iteration, 1);
    switch (it) {
    case 1:
        errno = EIO;
        result = -1;
        break;
    default:
        result = original_unlink(file);
        break;
    }

    printf("file: %s, current unlink iteration = %d\n", file, it);
    return result;
}

static int my_unlink(const char *file)
{
    int result = 0;
    result = test_unlink(file);
    return result;
}

static int expect_error = 0;
static void my_error_fun(int e, const char *s, void *ignore) {
    check(ignore==NULL);
    fprintf(stderr, "Got error %d (I expected errno=%d) (%s)\n", e, expect_error, s);
}

static int test_unlink_failures(void)
{
    int result = 0;
    // 1.  Create the file.
    const char * src = get_src();
    const int FILE_NAME_SIZE = 100;
    char file[FILE_NAME_SIZE] = {0};
    snprintf(file, FILE_NAME_SIZE, "%s/my.data", src);
    int fd = openf(O_RDWR | O_CREAT, 0777, "%s", file);
    check(fd >= 0);
    const int SIZE = 10;
    char buf[SIZE] = {0};
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
                                  EIO);

    while (!backup_is_capturing()) { sched_yield(); }
    sleep(2);

    // 4.  Try to rename the created file.
    expect_error = EIO;
    int unlink_r = unlink(file);
    check(unlink_r == 0);

    // 5. Cleanup
    backup_set_keep_capturing(false);
    finish_backup_thread(backup_thread);
    free((void*) src);
    return result;
}

int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    int result = 0;
    setup_source();
    setup_destination();
    original_unlink = register_unlink(my_unlink);
    result = test_unlink_failures();
    cleanup_dirs();
    return result;
}
