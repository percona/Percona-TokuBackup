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

int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    char *src = get_src();
    char *dst = get_dst();
    setup_source();
    setup_destination();

    size_t len = strlen(src) + 20; 
    char *no_such_path = (char*)malloc(len);
    {
        size_t r = snprintf(no_such_path, len, "%s/no_such.data", src);
        check(r<len);
    }

    pthread_t thread;
    backup_set_start_copying(false);
    start_backup_thread(&thread);

    // The sequence of controlls is very stylized.  Do it wrong, and you'll have races in your test code.
    while(!backup_is_capturing()) sched_yield();
    backup_set_keep_capturing(true);
    backup_set_start_copying(true);
    int r = unlink(no_such_path);
    check(r==-1);
    while(!backup_done_copying()) sched_yield();
    backup_set_keep_capturing(false);
    while(!backup_is_capturing()) sched_yield();
    finish_backup_thread(thread);
    free(no_such_path);
    free(src);
    free(dst);
    return 0;
}
