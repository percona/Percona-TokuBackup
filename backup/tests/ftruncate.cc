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
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "backup_test_helpers.h"
#include "backup_internal.h"

//
void test_truncate(void) {
    int result = 0;
    setup_source();
    setup_dirs();
    setup_destination();

    char *src = get_src();

    backup_set_keep_capturing(true);
    pthread_t thread;
    start_backup_thread(&thread);

    // Create a new file.
    int fd = openf(O_CREAT | O_RDWR, 0777, "%s/my.data", src);
    check(fd >= 0);
    free(src);

    // Write a large chunk of data to it.
    result = write(fd, "Hello World", 12);

    // Truncate the end off.
    {
        int r = ftruncate(fd, 6);
        check(r==0);
    }
    backup_set_keep_capturing(false);
    const int SIZE = 20;
    char buf_source[SIZE];
    size_t src_n_read = pread(fd, buf_source, SIZE, 0);
    check(src_n_read==6);

    finish_backup_thread(thread);

    // Confirm that the both files are the same size.
    char *dst = get_dst();
    int backup_fd = openf(O_RDWR, 0, "%s/my.data", dst);
    check(backup_fd>=0);
    free(dst);
    char buf_copy[SIZE];
    size_t dst_n_read = read(backup_fd, buf_copy, SIZE);
    check(dst_n_read==6);
    result = memcmp(buf_source, buf_copy, src_n_read);

    if (result != 0) { 
        fail();
        printf(": BACKUP: <TEST> Files don't match! %s != %s\n", buf_source, buf_copy);
        fail();
    } else {
        pass();
    }

    printf(": test_truncate()\n");
}

int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    test_truncate();
    return 0;
}
