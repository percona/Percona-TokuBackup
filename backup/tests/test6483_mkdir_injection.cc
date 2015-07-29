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
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include "backup_test_helpers.h"
#include "backup_internal.h"
#include "real_syscalls.h"

static bool disable_injections = true;
static std::vector<long> injection_pattern; // On the Kth pwrite or write, return ENOSPC, if K is in this vector.
static long injection_write_count = 0;

static char *src;
static char *dst;
static char *realdst;

static bool inject_this_time(const char *path) {
    //printf("realdst=%s\npath   =%s\ninjection_write_count=%ld\n", realdst, path, injection_write_count);
    {
        struct stat buf;
        if (stat(path, &buf)==0) return false; // don't inject a failure when the path is already there.
    }
    if (strncmp(realdst, path, strlen(realdst))!=0) return false;
    long old_count = __sync_fetch_and_add(&injection_write_count,1);
    for (size_t i=0; i<injection_pattern.size(); i++) {
        if (injection_pattern[i]==old_count) {
            return true;
        }
    }
    return false;
}

static mkdir_fun_t original_mkdir;

static int my_mkdir(const char *path, mode_t mode) {
    //fprintf(stderr, "Doing mkdir(%s, %d)\n", path, mode); // ok to do a write, since we aren't further interposing writes in this test.
    if (inject_this_time(path)) {
        fprintf(stderr, "Injecting error\n");
        errno = ENOSPC;
        return -1;
    } else {
        return original_mkdir(path, mode);
    }
}

static int ercount=0;
static void my_error_fun(int e, const char *s, void *ignore) {
    check(ignore==NULL);
    ercount++;
    fprintf(stderr, "Got error %d (I expected errno=%d) (%s)\n", e, ENOSPC, s);
}
    
static void testit(int expect_error) {
    disable_injections = true;
    injection_write_count = 0;

    setup_source();
    setup_dirs();
    setup_destination();

    if (!src) {
        src = get_src();
        dst = get_dst();
        realdst = realpath(dst, NULL);
        check(realdst);
    }

    disable_injections = false;

    backup_set_start_copying(false);
    backup_set_keep_capturing(true);

    pthread_t thread;

    start_backup_thread_with_funs(&thread,
                                  get_src(), get_dst(),
                                  simple_poll_fun, NULL,
                                  my_error_fun, NULL,
                                  expect_error);
    while(!backup_is_capturing()) sched_yield(); // wait for the backup to be capturing.
    //fprintf(stderr, "The backup is supposedly capturing\n");
    {
        char s[1000];
        snprintf(s, sizeof(s), "%s/dir0", src);
        int r = mkdir(s, 0777);
        check(r==0);
    }

    {
        char s[1000];
        snprintf(s, sizeof(s), "%s/dir0/dir1", src);
        int r = mkdir(s, 0777);
        check(r==0);
    }
    fprintf(stderr,"About to start copying\n");
    backup_set_start_copying(true);

    backup_set_keep_capturing(false);
    finish_backup_thread(thread);
}


int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    original_mkdir  = register_mkdir(my_mkdir);

    for (int i=0; i<7; i++) {
        printf("test #%d\n", i);
        injection_pattern.resize(0);
        injection_pattern.push_back(i);
        testit((i<3) ? ENOSPC : 0);
        printf("Done #%d\n", i);
    }

    free(src);
    free(dst);
    free(realdst);
    return 0;
}

// Instantiate what we need
template class std::vector<long>;
