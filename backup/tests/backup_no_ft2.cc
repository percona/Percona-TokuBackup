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

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "backup_test_helpers.h"
#include "backup_internal.h"

char *src, *dst;

static int fds[4];
static void open_n(int n) {
    fds[n] = openf(O_CREAT | O_WRONLY, S_IRWXU | S_IRWXG | S_IRWXO, "%s/file%d", src, n);
    check(fds[n]>=0);
}
static void close_n(int n) {
    int r = close(fds[n]);
    check(r==0);
    fds[n] = 0;
}
static void write_n(int n) {
    char data[100];
    snprintf(data, sizeof(data), "%d", n);
    const size_t len = strlen(data);
    const ssize_t r = write(fds[n], data, len);
    check(r == (ssize_t)len);
}

int test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    setup_destination();
    setup_source();
    src = get_src();
    dst = get_dst();

    backup_set_keep_capturing(true);
    pthread_t thread;
    start_backup_thread(&thread);

    open_n(0);
    write_n(0);

    open_n(1);
    write_n(1);

    open_n(2);
    write_n(2);

    close_n(0);

    open_n(3);
    write_n(3);
    write_n(2);
    write_n(1);

    close_n(1);
    close_n(2);
    close_n(3);

    backup_set_keep_capturing(false);
    finish_backup_thread(thread);

    {
        int status = systemf("diff -r %s %s", src, dst);
        check(status!=-1);
        check(WIFEXITED(status));
        check(WEXITSTATUS(status)==0);
    }
    free(src);
    free(dst);
    return 0;
}
