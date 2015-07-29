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
#include "backup_debug.h"

const int SIZE = 100;
static int fd;
const char ONE = 'a';
const char ZERO = 'b';

static void* write_ones(void *p __attribute__((__unused__))) {
    void *ret = NULL;
    char data[SIZE] = {ONE};    
    int r = pwrite(fd, data, SIZE, 0);
    check(r == SIZE);
    return ret;
}

static int verify(void)
{
    int result = 0;
    char *source_scratch = get_src();
    char *destination_scratch = get_dst();
    char source_file[SIZE];
    snprintf(source_file, SIZE, "%s/%s", source_scratch, "MyFile.txt");
    char destination_file[SIZE];
    snprintf(destination_file, SIZE, "%s/%s", destination_scratch, "MyFile.txt");
    int src_fd = open(source_file, O_RDWR);
    int dst_fd = open(destination_file, O_RDWR);

    char src_buf[SIZE];
    char dst_buf[SIZE];
    int r = pread(src_fd, src_buf, SIZE, 0);
    check(r == SIZE);
    r = pread(dst_fd, dst_buf, SIZE, 0);
    check(r == SIZE);

    if (src_buf[0] != dst_buf[0]) {
        printf("\nCharacters in Buffers don't match: %c vs %c\n", src_buf[0], dst_buf[0]);
        result = -1;
    }
    
    close(dst_fd);
    close(src_fd);
    free((void*)destination_scratch);
    free((void*)source_scratch);
    return result;
}

static int write_race(void) {
    int result = 0;

    // 1. Create one file in source with a few bytes.
    char file[100];
    char *free_me = get_src();
    snprintf(file, 100, "%s/%s", free_me, "MyFile.txt");
    fd = open(file, O_CREAT | O_RDWR, 0777);
    check(fd >= 0);
    char data[SIZE] = {ZERO};
    int r = write(fd, data, SIZE);
    check(r == SIZE);

    // 2. Set Pause point to after read but before write
    HotBackup::toggle_pause_point(HotBackup::COPIER_AFTER_READ_BEFORE_WRITE);

    // 3. Start backup.
    pthread_t backup_thread;
    start_backup_thread(&backup_thread);
    sleep(2); // Give the copier a change to grab the lock.

    // 6. start new thread that writes to the first few bytes.
    pthread_t write_thread;
    r = pthread_create(&write_thread, NULL, write_ones, NULL);
    check(r == 0);

    // 7. Sleep for a little while to give the thread a chance to write and/or get blocked.
    sleep(2);

    // 8. Turn off pause point.  Allows copier to unlock file,  allowing writer to finish.
    HotBackup::toggle_pause_point(HotBackup::COPIER_AFTER_WRITE);
    HotBackup::toggle_pause_point(HotBackup::COPIER_AFTER_READ_BEFORE_WRITE);

    r = pthread_join(write_thread, NULL);
    check(r == 0);
    HotBackup::toggle_pause_point(HotBackup::COPIER_AFTER_WRITE);
    finish_backup_thread(backup_thread);

    // 9. Check that backup copy and source copy match.

    result = verify();
    if (result != 0) {
        fail();
    } else {
        pass();
    }

    // 10. cleanup:
    close(fd);
    free((void*)free_me);
    return result;
}

int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    int result = 0;
    setup_source();
    setup_destination();
    result = write_race();
    cleanup_dirs();
    return result;
}
