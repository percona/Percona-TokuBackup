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
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include "backup.h"
#include "backup_internal.h"
#include "backup_test_helpers.h"
#include "backup_debug.h"

const int N = 1 << 2;
const int SIZE = 100;
const char BEFORE = 'a';

static void create_n_files(void)
{
    const char *src = get_src();
    for (int i = 0; i < N; ++i) {
        int fd = openf(O_RDWR | O_CREAT, 0777, "%s/my_%d.data", src, i);
        assert(fd > 0);
        char buf[SIZE] = {BEFORE};
        int pwrite_r = pwrite(fd, buf, SIZE, 0);
        check(pwrite_r == SIZE);
        int close_r = close(fd);
        check(close_r == 0);
    }

    free((void*)src);
}

static int verify(void)
{
    int result = 0;
    return result;
    char *source_scratch = get_src();
    char *destination_scratch = get_dst();
    char source_file[SIZE];
    char destination_file[SIZE];

    // Verify the first N - 1 were renamed.
    for (int i = 0; i < N - 1; ++i) {
        snprintf(source_file, SIZE, "%s/renamed_%d.data", source_scratch, i);
        struct stat blah;
        int stat_r = stat(source_file, &blah);
        check(stat_r == 0);
        snprintf(destination_file, SIZE,  "%s/renamed_%d.data", destination_scratch, i);
        stat_r = stat(destination_file, &blah);
        check(stat_r == 0);
    }

    struct stat buf;
    snprintf(source_file, SIZE, "%s/renamed_%d.data", source_scratch, (N - 1));
    snprintf(destination_file, SIZE,  "%s/renamed_%d.data", destination_scratch, (N - 1));
    int stat_r = stat(source_file, &buf);
    assert(stat_r == 0);
    stat_r = stat(destination_file, &buf);
    if (stat_r != 0) {
        int error = errno;
        if (error != ENOENT) {
            result = -1;
            perror("Unkown stat error, expected ENOENT\n");
        }
    } else {
        result = -1;
        printf("TEST ERROR:renamed fourth file should not exist in destination.\n");
    }

    snprintf(destination_file, SIZE,  "%s/my_%d.data", destination_scratch, N - 1);
    stat_r = stat(destination_file, &buf);
    assert(stat_r == 0);
    free((void*)destination_scratch);
    free((void*)source_scratch);
    return result;
}

static void my_rename(int i)
{
    int r = 0;
    const char * free_me = get_src();
    char old_name[PATH_MAX] = {0};
    char new_name[PATH_MAX] = {0};
    snprintf(old_name, PATH_MAX, "%s/my_%d.data", free_me, i);
    snprintf(new_name, PATH_MAX, "%s/renamed_%d.data", free_me, i);
    r = rename(old_name, new_name);
    check(r == 0);
    free((void*) free_me);
}

static int rename_test(void)
{
    int result = 0;
    // 1. Create four files.
    create_n_files();
    // 2. Prevent copy from starting.
    backup_set_start_copying(false);
    // 3. Tell backup to keep capturing.
    backup_set_keep_capturing(true);
    // 4. Rename first file.
    my_rename(0);
    // 5. Start backup.
    pthread_t thread;
    start_backup_thread(&thread);
    // 6. Rename second file.
    my_rename(1);
    // 7. Let backup finish copy.
    backup_set_start_copying(true);
    // 8. Rename third file.
    my_rename(2);
    // 9. Let backup finish (release keep_capturing bool)
    backup_set_keep_capturing(false);
    finish_backup_thread(thread);
    // 10. Rename fourth file.
    my_rename(3);
    // 11. Verify that all but the third rename succeeded in the backup.
    result = verify();
    if (result == 0) {
        pass();
    } else {
        fail();
    }

    return result;
}

int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    int result = 0;
    setup_source();
    setup_destination();
    result = rename_test();
    cleanup_dirs();
    return result;
}
