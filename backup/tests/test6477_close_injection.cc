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

/* Inject enospc. */

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <vector>

#include "backup_test_helpers.h"
#include "backup_internal.h"
#include "real_syscalls.h"

static bool disable_injections = true;
static std::vector<long> injection_pattern; // On the Kth pwrite or write, return ENOSPC, if K is in this vector.
static std::vector<int>  ignore_fds;        // Don't count pwrite or write to any fd in this vector.
static long injection_write_count = 0;

static bool inject_this_time(int fd) {
    if (disable_injections) return false;
    for (size_t i=0; i<ignore_fds.size(); i++) {
        if (ignore_fds[i]==fd) return false;
    }
    long old_count = __sync_fetch_and_add(&injection_write_count,1);
    for (size_t i=0; i<injection_pattern.size(); i++) {
        if (injection_pattern[i]==old_count) {
            return true;
        }
    }
    return false;
}

static close_fun_t original_close;

static int my_close(int fd) {
    fprintf(stderr, "Doing close on fd=%d\n", fd); // ok to do a write, since we aren't further interposing writes in this test.
    if (inject_this_time(fd)) {
        fprintf(stderr, "Injecting error\n");
        original_close(fd); // close it.
        errno = EIO;
        return -1;
    } else {
        return original_close(fd);
    }
}

static int expect_error = 0;
static int ercount=0;
static void my_error_fun(int e, const char *s, void *ignore) {
    check(ignore==NULL);
    ercount++;
    fprintf(stderr, "Got error %d (I expected errno=%d) (%s)\n", e, expect_error, s);
}
    
static char *src;

static void testit(int expect) {
    expect_error = expect;
    disable_injections = true;
    injection_write_count = 0;

    setup_source();
    setup_dirs();
    setup_destination();

    disable_injections = false;

    backup_set_start_copying(false);
    backup_set_keep_capturing(true);

    pthread_t thread;

    int fd = openf(O_RDWR|O_CREAT, 0777, "%s/my.data", src);
    check(fd>=0);
    fprintf(stderr, "fd=%d\n", fd);
    ignore_fds.push_back(fd);

    start_backup_thread_with_funs(&thread,
                                  get_src(), get_dst(),
                                  simple_poll_fun, NULL,
                                  my_error_fun, NULL,
                                  expect);
    while(!backup_is_capturing()) sched_yield(); // wait for the backup to be capturing.
    fprintf(stderr, "The backup is supposedly capturing\n");
    {
        ssize_t r = pwrite(fd, "hello", 5, 10);
        check(r==5);
    }
    fprintf(stderr,"About to start copying\n");
    backup_set_start_copying(true);
    {
        int r = close(fd);
        check(r==0);
    }

    backup_set_keep_capturing(false);
    finish_backup_thread(thread);
}


int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    src = get_src();
    original_close = register_close(my_close);

    const int N = 1;
    for (int i=0; i<N; i++) {
        printf("TEST %d\n", i);
        injection_pattern.resize(0);
        injection_pattern.push_back(i);
        testit(EIO);
    }

    printf("Final test\n");
    injection_pattern.resize(0);
    injection_pattern.push_back(N);
    testit(0);

    free(src);
    return 0;
}

// Instantiate what we need
template class std::vector<int>;
template class std::vector<long>;
