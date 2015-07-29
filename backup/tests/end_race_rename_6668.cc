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

/* Create a race at the end */

#include "backup_internal.h"
#include "backup_test_helpers.h"
#include "real_syscalls.h"

#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char *src, *dst;
static char *delay_this_old;

static rename_fun_t original_rename;
static int my_rename(const char *oldpath, const char *newpath) {
    printf("Rename\n %s\n %s (delaying\n %s)\n", oldpath, newpath, delay_this_old);
    bool delay_it = (strcmp(delay_this_old, oldpath)==0);
    printf("delay=%d\n", delay_it);
    //    printf("my_open(\"%s\", %d,...) delay=%d\n", path, flags, delay_it);
    //    if (delay_it) {
    //        backup_set_keep_capturing(false); // inside the open we let the capture finish, then sleep
    //        sleep(1);
    //        printf("slept.\n");
    //    }
    int r = original_rename(oldpath, newpath);
    if (delay_it) {
        backup_set_keep_capturing(false);
        sleep(1);
    }
    return r;
}

static void* do_rename(void* ignore) {
    {
        int fd = openf(O_RDWR | O_CREAT, 0777, "%s/old.data", src);
        check(fd>=0);
        int r = close(fd);
        check(r==0);
    }

    size_t len = strlen(src) + 10; 
    char *oldpath = (char*)malloc(len);
    char *newpath = (char*)malloc(len);
    {
        size_t r = snprintf(oldpath, len, "%s/old.data", src);
        check(r<len);
    }
    char *old_realpath = realpath(oldpath, NULL);
    {
        size_t r = snprintf(newpath, len, "%s/new.data", src);
        check(r<len);
    }

    // This sequence is very stylized.  Do it wrong, and you'll have races in your test code.
    while(!backup_is_capturing()) sched_yield();
    backup_set_keep_capturing(true);
    backup_set_start_copying(true);
    while(!backup_done_copying()) sched_yield();
    backup_set_keep_capturing(false);
    while(!backup_is_capturing()) sched_yield();

    int r = rename(old_realpath, newpath);
    check(r==0);
    free(old_realpath);
    free(oldpath);
    free(newpath);
    return ignore;
}

int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    src = get_src();
    dst = get_dst();
    setup_source();
    setup_destination();
    {
        char *absdst = realpath(dst, NULL);
        size_t l = strlen(absdst) + 10;
        delay_this_old = (char*)malloc(l);
        size_t r = snprintf(delay_this_old, l, "%s/old.data", absdst);
        check(r<l);
        free(absdst);
    }
    original_rename = register_rename(my_rename);
    pthread_t thread;
    pthread_t open_th;
    backup_set_start_copying(false);
    start_backup_thread(&thread);
    {
        int r = pthread_create(&open_th, NULL, do_rename, NULL);
        check(r==0);
    }
    finish_backup_thread(thread);
    cleanup_dirs(); // try to delete things out from under the running open.
    {
        void *result;
        int r = pthread_join(open_th, &result);
        check(r==0 && result==NULL);
    }
    free(src);
    free(dst);
    free(delay_this_old);
    return 0;
}
