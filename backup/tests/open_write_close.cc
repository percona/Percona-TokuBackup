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
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "backup_helgrind.h"

#include "backup.h"
#include "backup_test_helpers.h"

const char * const WRITTEN_STR = "goodbye\n";
const int WRITTEN_STR_LEN = 8;

//
static int verify(void)
{
    char *dst = get_dst();
    int backup_fd = openf(O_RDONLY, 0, "%s/bar.data", dst);
    check(backup_fd >= 0);    
    free(dst);
    char backup_string[30] = {0};
    int r = read(backup_fd, backup_string, WRITTEN_STR_LEN);
    check(r == WRITTEN_STR_LEN);
    r = strcmp(WRITTEN_STR, backup_string);
    return r;
}

volatile int write_done = 0;

static int write_poll(float progress, const char *progress_string, void *extra) {
    check(0<=progress && progress<1);
    check(extra==NULL);
    check(strlen(progress_string)>8);
    while (!write_done) {
        sched_yield();
    }
    return 0;
}


//
static void open_write_close(void) {
    TOKUBACKUP_VALGRIND_HG_DISABLE_CHECKING(&write_done, sizeof(write_done));

    setup_source();
    setup_dirs();
    setup_destination();
    pthread_t thread;
    start_backup_thread_with_funs(&thread, get_src(), get_dst(),
                                  write_poll, NULL,
                                  dummy_error, NULL,
                                  0);

    char *src = get_src();
    int fd = openf(O_WRONLY, 0, "%s/bar.data", src);
    check(fd >= 0);
    free(src);
    int result = write(fd, WRITTEN_STR, WRITTEN_STR_LEN);
    check(result == 8);
    write_done = 1; // let the poll return, so that the backup will not finish before this write took place.
    result = close(fd);
    check(result == 0);

    finish_backup_thread(thread);

    if(verify()) {
        fail();
    } else {
        pass();
    }
    printf(": open_write_close()\n");
}

int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    open_write_close();
    return 0;
}
