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

#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "backup_test_helpers.h"
#include "backup_internal.h"
#include "real_syscalls.h"

int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    setup_source();
    setup_dirs();
    setup_destination();

    char *src = get_src();

    backup_set_keep_capturing(true);
    pthread_t thread;

    const int n_fds = 20;
    int fds[n_fds];
    for (int i=0; i<n_fds; i++) {
        fds[i] = openf(O_RDWR|O_CREAT, 0777, "%s/my%d.data", src, i);
        check(fds[i]>=0);
    }
    
    start_backup_thread(&thread);

    backup_set_keep_capturing(false);
    usleep(1000);
    printf("said to keep capturing\n");

    int n_fds_left = n_fds;
    int count = 0;
    while (n_fds_left) {
        int idx = random()%n_fds_left;
        int fd = fds[idx];
        fds[idx] = fds[n_fds_left-1];
        n_fds_left--;

        char data[100];
        int len = snprintf(data, sizeof(data), "data_order %d\n", count);
        {
            ssize_t r = write(fd, data, len);
            check(r==len);
        }
        {
            int r = close(fd);
            check(r==0);
        }
        count++;
        
    }
    
    finish_backup_thread(thread);

    free(src);

    return 0;
}
