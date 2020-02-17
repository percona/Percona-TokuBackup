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

// Test that opens, lseeks, and writes are intercepted by backup and executed while backup is
// still capturing.

#ident "$Id$"

#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "backup.h"
#include "backup_test_helpers.h"

static int verify(void) {
    char *src = get_src();
    char *dst = get_dst();
    int r = systemf("diff -r %s %s", src, dst);
    free(src);
    free(dst);
    if (!WIFEXITED(r)) return -1;
    if (WEXITSTATUS(r)!=0) return -1;
    return 0;
}

int lseek_write_after_copy(void) {
    int result = 0;
    int fd = 0;
    setup_source();
    setup_dirs();
    setup_destination();

    // keep backup capturing until after the test executes the
    // final lseek and write.
    backup_set_keep_capturing(true);

    // start backup
    pthread_t thread;
    start_backup_thread(&thread);

    // wait for backup copy phase to copy the source file
    while (!backup_done_copying()) {
        sched_yield();
    }

    // create a source file and write at the beginning of the file
    char *src = get_src();
    fd = openf(O_CREAT | O_RDWR, 0777, "%s/my.data", src);
    check(fd >= 0);
    free(src);
    result = write(fd, "Hello World\n", 12);
    check(result == 12);

    // write at 1M offset
    const off_t next_write_offset = 1<<20;
    result = lseek(fd, next_write_offset, SEEK_SET);
    check(result == next_write_offset);
    result = write(fd, "Cruel World\n", 12);
    check(result == 12);

    result = close(fd);
    check(result == 0);

    // wait for the backup to finish
    backup_set_keep_capturing(false);
    finish_backup_thread(thread);

    // verify source and backup files
    if(verify()) {
        fail(); result = 1;
    } else {
        pass(); result = 0;
    }
    return result;
}

int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    int result = lseek_write_after_copy();
    return result;
}
