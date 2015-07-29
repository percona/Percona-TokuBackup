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

#include "backup_test_helpers.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static char *src;
static char *dst;


int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    src = get_src();
    dst = get_dst();
    setup_source();
    setup_destination();
    setup_dirs();

    pthread_t thread;
    const int N = 2;
    const int BUFSIZE = 1024;
    const int NBUFS   = 1024;
    unsigned char buf[BUFSIZE];
    for (size_t i=0; i<sizeof(buf); i++) buf[i]=i%256;
    for (int i=0; i<N; i++) {
        char fname[1000];
        {
            int r = snprintf(fname, sizeof(fname), "%s/f%d", src, i);
            check(r<(int)sizeof(fname));
        }
        int fd = open(fname, O_WRONLY|O_CREAT, 0777);
        check(fd>=0);
        for (int j=0; j<NBUFS; j++) {
            ssize_t r = write(fd, buf, sizeof(buf));
            check(r==sizeof(buf));
        }
        {
            int r = close(fd);
            check(r==0);
        }
    }

    const long throttle = 1L<<18;  // quarter mebibyte per second.
    double expected_n_seconds = (N * BUFSIZE * NBUFS)/(double)throttle;
    tokubackup_throttle_backup(1L<<18); // quarter mebibyte per second, so that's 4 seconds.
    struct timespec start,end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    start_backup_thread(&thread);
    finish_backup_thread(thread);
    clock_gettime(CLOCK_MONOTONIC, &end);
    double td = tdiff(start, end);
    printf("time used     == %6.3fs\n", td);
    printf("time expected >= %6.3fs\n", expected_n_seconds);
    check(td >= expected_n_seconds);

    free(src);
    free(dst);
    return 0;
}

