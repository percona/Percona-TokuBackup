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
#include "backup_internal.h"
#include "backup_test_helpers.h"
#include "backup_debug.h"

const int SIZE = 100;
const char ONE = 'a';
const char ZERO = 'b';
const unsigned int ASCII_OFFSET = 48;
const int N = 16;
static int fd_array[N] = {0};

static void* write_ones(void *p) {
    const int * const nth_ptr = (const int * const)p;
    const int nth_fd = *nth_ptr;
    char data[SIZE] = {ONE};
    int pwrite_r = write(fd_array[nth_fd], data, SIZE);
    check(pwrite_r == SIZE);
    return NULL;
}

static int verify(void)
{
    int result = 0;
    char *source_scratch = get_src();
    char *destination_scratch = get_dst();
    for (int i = 0; i < N; ++i) {
        char c = (char) i + ASCII_OFFSET;
        char source_file[SIZE];
        snprintf(source_file, SIZE, "%s/%c%c%c%c", source_scratch, c, c, c, c);
        char destination_file[SIZE];
        snprintf(destination_file, SIZE, "%s/%c%c%c%c", destination_scratch, c, c, c, c);
        int src_fd = open(source_file, O_RDWR);
        int dst_fd = open(destination_file, O_RDWR);

        char src_buf[SIZE];
        char dst_buf[SIZE];
        int r = pread(src_fd, src_buf, SIZE, 0);
        check(r == SIZE);
        r = pread(dst_fd, dst_buf, SIZE, 0);
        check(r == SIZE);

        if (i == N || i == 0) {
            if (src_buf[0] == dst_buf[0]) {
                printf("\nCharacters in Buffers should NOT  match: %c vs %c\n",
                       src_buf[0],
                       dst_buf[0]);
                result = -1;
            }
        }
        printf("file=%s, src[0]=%c, dst[0]=%c\n", source_file, src_buf[0], dst_buf[0]);

        close(dst_fd);
        close(src_fd);
    }
    
    free((void*)destination_scratch);
    free((void*)source_scratch);
    return result;
}

static void create_n_files(void)
{
    char file[SIZE];
    char *source_scratch = get_src();
    for (int i = 0; i < N; ++i) {
        char c = (char) i + ASCII_OFFSET;
        snprintf(file, SIZE, "%s/%c%c%c%c", source_scratch, c, c, c, c);
        printf("%s\n", file);
        fd_array[i] = open(file, O_CREAT | O_RDWR, 0777);
        check(fd_array[i] >= 0);
        char data[SIZE] = {ZERO};
        int r = write(fd_array[i], data, SIZE);
        check(r == SIZE);
    }

    free((void *) source_scratch);
}

static int disable_race(void) {
    int result = 0;

    // 1. Create N files with some data.
    create_n_files();
    lseek(fd_array[0], 0, SEEK_SET);
    lseek(fd_array[N - 1], 0, SEEK_SET);

    // 2. Set Pause Point to mid point of disabling descriptions.
    backup_pause_disable(true);
    //backup_set_keep_capturing(true);

    // 3. Start backup
    pthread_t backup_thread;
    start_backup_thread(&backup_thread);
    printf("NOTE: Waiting for backup thread to finish copy phase...\n");
    sleep(4); // 2 seconds wasn't enough under helgrind with a high CPU load.  This really should have a pause point to make sure that the right thing happens.

    // 7. Create two threads, writing to the 0th and Nth files respectively. 
    printf("NOTE: Creating write threads.\n");

    pthread_t write_zero_thread;
    int first = 0;
    int r = pthread_create(&write_zero_thread, NULL, write_ones, &first); 
    check(r == 0);

    pthread_t write_n_thread;
    int second = N - 1;
    r = pthread_create(&write_n_thread, NULL, write_ones, &second);
    check(r == 0);
    printf("NOTE: Created write threads, waiting to hit critical section\n");
    sleep(1);

    // 8. Disable Pause Point.
    printf("NOTE: Allowing disabling to continue.\n");
    backup_pause_disable(false);

    r = pthread_join(write_n_thread, NULL);
    check(r == 0);
    r = pthread_join(write_zero_thread, NULL);
    check(r == 0);
    finish_backup_thread(backup_thread);

    // 9. Verify that neither write made it to the backup file.
    result = verify();
    if (result != 0) {
        fail();
    } else {
        pass();
    }

    // Close all the open files.
    for (int i = 0; i < N; ++i) {
        int rr = close(fd_array[i]);
        check(rr == 0);
    }

    return result;
}

int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    int result = 0;
    setup_source();
    setup_destination();
    result = disable_race();
    cleanup_dirs();
    return result;
}
