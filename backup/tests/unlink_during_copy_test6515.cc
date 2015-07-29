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
#include <unistd.h>
#include "backup_test_helpers.h"

int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    char *src = get_src();
    char *dst = get_dst();
    setup_source();
    setup_destination();
    setup_dirs();
    pthread_t thread;
    const int N = 2;
    char fname[N][1000];
    int fds[N];
    unsigned char buf[1024];
    for (size_t i=0; i<sizeof(buf); i++) buf[i]=i%256;
    for (int i=0; i<N; i++) {
        {
            int r = snprintf(fname[i], sizeof(fname[i]), "%s/f%d", src, i);
            printf("r=%d fname[%d]=%s\n", r, i, fname[i]);
            check(r<(int)sizeof(fname[i]));
        }
        fds[i] = open(fname[i], O_WRONLY|O_CREAT, 0777);
        check(fds[i]>=0);
        for (int j=0; j<1024; j++) {
            ssize_t r = write(fds[i], buf, sizeof(buf));
            check(r==sizeof(buf));
        }
    }
    tokubackup_throttle_backup(1L<<19); // half a mebibyte per second, so that's 2 seconds.
    start_backup_thread(&thread);
    for (int i=0; i<N; i++) {
        int r = unlink(fname[i]);
        check(r==0);
    }
    finish_backup_thread(thread);
    for (int i=0; i<N; i++) {
        int r = close(fds[i]);
        check(r==0);
    }
    {
        int status = systemf("diff -r %s %s", src, dst);
        check(status!=-1);
        check(WIFEXITED(status));
        printf("status=%d\n", status);
        check(WEXITSTATUS(status)==0);
    }
    free(src);
    free(dst);
    return 0;
}
