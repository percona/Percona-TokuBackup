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

void read_and_seek(void) {
    int result = 0;
    int fd = 0;
    setup_source();
    setup_dirs();
    setup_destination();
    pthread_t thread;
    start_backup_thread(&thread);

    char *src = get_src();
    fd = openf(O_CREAT | O_RDWR, 0777, "%s/my.data", src);
    check(fd >= 0);
    free(src);
    result = write(fd, "Hello World\n", 12);
    char buf[10];
    result = read(fd, buf, 5);
    if (result < 0) {
        perror("read failed.");
        abort();
    }

    result = lseek(fd, 1, SEEK_CUR);
    check(result > 0);
    result = write(fd, "Cruel World\n", 12);
    check(result > 0);
    result = close(fd);
    check(result == 0);

    finish_backup_thread(thread);

    if(verify()) {
        fail();
    } else {
        pass();
    }
    printf(": read_and_seek()\n");
}

int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    read_and_seek();
    return 0;
}
