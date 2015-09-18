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

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include "backup_helgrind.h"

#include "backup_test_helpers.h"
#include "real_syscalls.h"

static const int expect_error = ENOMEM;
static const int realpath_error_to_inject = expect_error;
static pthread_t backup_thread;

static void my_error_fun(int e, const char *s, void *ignore) 
{
    check(ignore==NULL);
    fprintf(stderr, "Got error %d (I expected errno=%d) (%s)\n", e, expect_error, s);
}

void start_backup(void)
{
    backup_set_keep_capturing(true);
    start_backup_thread_with_funs(&backup_thread,
                                  get_src(),
                                  get_dst(),
                                  simple_poll_fun,
                                  NULL,
                                  my_error_fun,
                                  NULL,
                                  expect_error);
    sleep(1);
    while (!backup_done_copying()) { sched_yield(); }
}

void finish_backup(void)
{
    backup_set_keep_capturing(false);
    finish_backup_thread(backup_thread);
}

const mode_t CREATE_FLAGS = 0777;

int call_create(const char * const file)
{
    int fd = open(file, O_CREAT | O_RDWR, CREATE_FLAGS);
    return fd;
}

int call_open(const char * const file)
{
    int fd = open(file, O_RDWR);
    return fd;
}

void call_rename(const char * const old_file, const char * const new_file)
{
    int r = rename(old_file, new_file);
    if (r != 0) {
        
    }
}

void call_unlink(const char * const file)
{
    int r = unlink(file);
    if (r != 0) {
        
    }
}

static realpath_fun_t original_realpath = NULL;
volatile static bool inject_realpath_error = false;
char *my_realpath(const char *path, char *result) {
    if (inject_realpath_error) {
        errno = realpath_error_to_inject;
        return NULL;
    } else {
        return original_realpath(path, result);
    }
}

int test_main(int n, const char **p)
{
    TOKUBACKUP_VALGRIND_HG_DISABLE_CHECKING(&inject_realpath_error, sizeof(inject_realpath_error));
    original_realpath = register_realpath(my_realpath);

    n++;
    p++;
    setup_source();
    setup_destination();

    int fd = 0;
    char file[PATH_MAX] = {0};
    char * src = get_src();
    snprintf(file, PATH_MAX, "%s/%s", src, "foo.txt");
    const char * dummy = call_real_realpath(file, NULL);
    if (dummy == NULL) {;}

    // 1. create
    printf("create...\n");
    start_backup();
    inject_realpath_error = true;
    fd = call_create(file);
    inject_realpath_error = false;
    close(fd);
    finish_backup();

    // 2. open
    start_backup();
    printf("open...\n");
    inject_realpath_error = true;
    fd = call_open(file);
    inject_realpath_error = false;
    finish_backup();
    close(fd);

    // 3. rename
    printf("rename...\n");
    setup_destination();
    start_backup();
    inject_realpath_error = true;
    call_rename(file, file);
    inject_realpath_error = false;
    finish_backup();

    // 4. unlink
    printf("unlink...\n");
    setup_destination();
    start_backup();
    inject_realpath_error = true;
    call_unlink(file);
    inject_realpath_error = false;
    finish_backup();

    cleanup_dirs();
    free(src);
    return 0;
}
