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

#include "backup.h"
#include "backup_test_helpers.h"

const int TRIES = 3;
const int LEN = 26;
char *dirs[TRIES];

static void setup(void)
{
    for (int i = 0; i < TRIES; ++i) {
        char s[LEN];
        int r = snprintf(s, sizeof(s), "multiple_backups.backup_%d", i);
        check(r<(int)sizeof(s));
        systemf("rm -rf %s", s);
        dirs[i] = strdup(s);
        check(dirs[i]);
    }
}

static void breakdown(void)
{
    // The backup thread helpers free dirs[i].
}

static void write_lots_of_dummy_data(char *src)
{
    const int SIZE = 100;
    const int MB = 1024 * 1024;
    char dummy[MB] = {0};
    char file[SIZE];
    snprintf(file, SIZE, "%s/%s", src, "dummy");
    int fd = open(file, O_CREAT | O_RDWR, 0777);
    check(fd >= 0);
    const int MEGS = 10;
    for (int i = 0; i < MEGS; ++i) {
        int r = write(fd, dummy, MB);
        check(r == MB);
    }
}

static void work_it(char *src, char *magic, int size)
{
    char file[100];
    snprintf(file, 100, "%s/%s", src, magic);

    // 1.  Create a file.
    int fd = open(file, O_CREAT | O_RDWR, 0777);
    check(fd >= 0);
    // 2.  Write magic to the file.
    int r = write(fd, magic, size);
    check(r == size);

    // 3.  Close the file.
    close(fd);

    // 4.  Open the file.
    fd = open(file, O_RDWR);

    // 5.  Read from the file.
    char *buf = (char*)malloc(sizeof(char) * size);
    r = read(fd, buf, size);

    // 6.  Write magic to the file.
    lseek(fd, size, SEEK_SET);
    r = write(fd, magic, size);

    // 7.  pwrite magic to the file.
    r = pwrite(fd, magic, size, size * 3);

    // 8.  ftruncate magic from the file.
    r = ftruncate(fd, size * 3);

    // close the file.
    close(fd);
    free((void*)buf);
}

static int check_it(char *magic, int size, int count)
{
    int r = 0;
    for (int i = 0; i <= count; ++i) {
        int backup_fd = openf(O_RDONLY, 0, "multiple_backups.backup_%d/magic%d", count , i);
        char backup_buf[20] = {0};
        int result = pread(backup_fd, backup_buf, size, 0);
        char magic_buf[20] = {0};
        snprintf(magic_buf, size, "%s%d", magic, i);
        result = strcmp(backup_buf, magic_buf);
        if (result != 0) {
            printf("Couldn't match: source:%s vs backup:%s\n", magic_buf, backup_buf);
            r = -1;
        }
    }

    return r;
}

//
static int multiple_backups(void) {
    setup_source();
    setup();
    char * src = get_src();
    write_lots_of_dummy_data(src);

    int result = 0;

    pthread_t thread;
    tokubackup_throttle_backup(1L << 22);
    for (int i = 0; i < TRIES; ++i) {
        setup_directory(dirs[i]);
        start_backup_thread(&thread, dirs[i]);
        const int SIZE = 7;
        char buf[SIZE] = {0};
        snprintf(buf, SIZE, "magic%i", i);
        work_it(src, buf, SIZE);
        finish_backup_thread(thread);
        snprintf(buf, SIZE, "magic");
        int r = check_it(buf, SIZE, i);
        if (r != 0) {
            result = -1 * i;
        }
    }

    free(src);
    if(result < 0) {
        printf("Backup # %d failed.\n", result * -1);
        fail();
        result = -1;
    } else {
        pass();
    }

    breakdown();

    return result;
}

int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    return multiple_backups();
}
