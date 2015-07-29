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
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "backup_test_helpers.h"
#include "backup_internal.h"

char *src, *dst;

#define FIRSTBYTES "first bytes\n"
#define MOREBYTES  "more bytes\n"

int test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    setup_destination();
    setup_source();
    src = get_src();
    dst = get_dst();

    int fd0 = openf(O_CREAT | O_WRONLY, S_IRWXU | S_IRWXG | S_IRWXO, "%s/file0", src);
    check(fd0>=0);
    {
        ssize_t r = write(fd0, FIRSTBYTES, sizeof(FIRSTBYTES)-1);
        check(r==sizeof(FIRSTBYTES)-1);
    }
    backup_set_keep_capturing(true);
    pthread_t thread;
    start_backup_thread(&thread);
    usleep(100000);
    {
        ssize_t r = write(fd0, MOREBYTES, sizeof(MOREBYTES)-1);
        check(r==sizeof(MOREBYTES)-1);
    }
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
