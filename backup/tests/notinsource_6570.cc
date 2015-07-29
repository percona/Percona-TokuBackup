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

// Test doing operations that are in a directory that's not backed up.

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "backup_test_helpers.h"

static char *not_src;

static void exercise(void) {
    size_t nlen = strlen(not_src)+10;
    // open, write, close, unlink
    {
        char filea_name[nlen];
        size_t slen = snprintf(filea_name, nlen, "%s/filea", not_src);
        assert(slen<nlen);
        int fda = open(filea_name, O_RDWR|O_CREAT, 0777);
        assert(fda>=0);
        {
            ssize_t r = write(fda, filea_name, slen);
            check(r==(ssize_t)slen);
        }
        {
            int r = close(fda);
            check(r==0);
        }
        {
            int r = unlink(filea_name);
            check(r==0);
        }
    }
    // open, unlink, write, close
    {
        char filea_name[nlen];
        size_t slen = snprintf(filea_name, nlen, "%s/filea", not_src);
        check(slen<nlen);
        int fda = open(filea_name, O_RDWR|O_CREAT, 0777);
        check(fda>=0);
        {
            int r = unlink(filea_name);
            check(r==0);
        }
        {
            ssize_t r = write(fda, filea_name, slen);
            check(r==(ssize_t)slen);
        }
        {
            int r = close(fda);
            check(r==0);
        }
    }
    // open, write, rename, close2
    {
        char filea_name[nlen];
        char fileb_name[nlen];
        size_t alen = snprintf(filea_name, nlen, "%s/filea", not_src);  check(alen<nlen);
        size_t blen = snprintf(fileb_name, nlen, "%s/fileb", not_src);  check(blen<nlen);
        int fda = open(filea_name, O_RDWR|O_CREAT, 0777);
        check(fda>=0);
        {
            ssize_t r = write(fda, filea_name, alen);
            check(r==(ssize_t)alen);
        }
        {
            int r = rename(filea_name, fileb_name);
            check(r==0);
        }
        {
            int r = close(fda);
            check(r==0);
        }
    }
}

int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    char *src = get_src();
    size_t slen = strlen(src);
    const char N_EXTRA_BYTES = 10;
    not_src = (char*)malloc(slen+N_EXTRA_BYTES);
    strncpy(not_src, src, slen+N_EXTRA_BYTES);
    const char go_back_n_bytes = 7;
    printf("backed up =%s\n",not_src+slen-go_back_n_bytes);
    check(0==strcmp(not_src+slen-go_back_n_bytes, ".source"));
    size_t n_written = snprintf(not_src+slen - go_back_n_bytes, N_EXTRA_BYTES+go_back_n_bytes, ".notsource");
    check(n_written< N_EXTRA_BYTES+go_back_n_bytes);
    {
        int r = systemf("rm -rf %s", not_src);
        check(r==0);
    }
    {
        int r = mkdir(not_src, 0777);
        check(r==0);
    }
    setup_source();
    setup_destination();

    int fd = openf(O_WRONLY|O_CREAT, 0777, "%s/data", src);
    check(fd>=0);
    {
        const int bufsize=1024;
        const int nbufs  =1024;
        char buf[bufsize];
        for (int i=0; i<bufsize; i++) {
            buf[i]=i%256;
        }
        for (int i=0; i<nbufs; i++) {
            ssize_t r = write(fd, buf, bufsize);
            check(r==bufsize);
        }
    }
    tokubackup_throttle_backup(1L<<18); // quarter megabyte per second, so that's 4 seconds.
    pthread_t thread;
    start_backup_thread(&thread);
    while (!backup_thread_is_done()) {
        exercise();
    }
    exercise();
    finish_backup_thread(thread);
    // And do two more.
    exercise();
    exercise();
    free(src);
    free(not_src);
    return 0;
}
