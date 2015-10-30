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

#include <assert.h>
#include <errno.h>
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

typedef int (*backup_exclude_copy_fun_t)(const char *source_file,void *extra);

static int exclude_all(const char *source_file __attribute__((__unused__)),
                       void *extra __attribute__((__unused__)))
{
    return !0;
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

static int check_it(char *dst, char *magic, int size, int count)
{
    int r = 0;
    int backup_fd = openf(O_RDONLY, 0, "%s/magic%d", dst, count);
    /***********/
    // We shouldn't find any files ever...
    int _errno_ = errno;
    if (backup_fd < 0) {
        perror("check_it() could not open destination file");
        printf("Expected file %s/magic%d apparently excluded from backup.\n", dst, count);
        assert(_errno_ == ENOENT);
        return 0;
    } else {
        printf("FAILURE: check_it() was able to open file %s/magic%d\n.", dst, count);
        return -1;
    }
    /**********/
    char backup_buf[20] = {0};
    int result = pread(backup_fd, backup_buf, size, 0);
    char magic_buf[20] = {0};
    snprintf(magic_buf, size, "%s%d", magic, count);
    result = strcmp(backup_buf, magic_buf);
    if (result != 0) {
        printf("Couldn't match: source:%s vs backup:%s\n", magic_buf, backup_buf);
        r = -1;
    }

    return r;
}

const int SIZE = 7;
char buf[SIZE] = {0};

struct SourceAndDestinationDirectories {
    int directory_count;
    char **sources;
    char **destinations;
    void Setup() {
        this->sources = (char**)malloc(sizeof(char**) * directory_count);
        this->destinations = (char**)malloc(sizeof(char**) * directory_count);
        for (int i = 0; i < this->directory_count; ++i) {
            this->sources[i] = get_src(i);
            this->destinations[i] = get_dst(i);
            setup_directory(this->sources[i]);
            setup_directory(this->destinations[i]);
        }
    }

    void Cleanup() {
        // Don't forget to deallocate the strings!
        for (int i = 0; i < this->directory_count; ++i) {
                free((void*)this->sources[i]);
                free((void*)this->destinations[i]);
        }
        free(this->sources);
        free(this->destinations);
    }

    void WriteDummyData() {
        for (int i = 0; i < this->directory_count; ++i) {
            write_lots_of_dummy_data(this->sources[i]);
        }
    }

    void DoWorkInSourceDirectory() {
        for (int i = 0; i < this->directory_count; ++i) {
            snprintf(buf, SIZE, "magic%i", i);
            work_it(this->sources[i], buf, SIZE);
        }
    }

    int CheckWork() {
        int result = 0;
        for (int i = 0; i < this->directory_count; ++i) {
            snprintf(buf, SIZE, "magic");
            int r = check_it(this->destinations[i], buf, SIZE, i);
            if (r != 0) {
                result = -1 * i;
            }
        }

        return result;
    }
};


int many_directories(const int directory_count, const bool keep_capturing) {
    int result = 0;
    set_dir_count(directory_count);
    SourceAndDestinationDirectories dirs;
    dirs.directory_count = directory_count;
    dirs.Setup();
    pthread_t thread;
    if (keep_capturing) {
        backup_set_keep_capturing(true);
        start_backup_thread_with_exclusion_callback(&thread, exclude_all, NULL);
        sleep(2); // Let Copy phase finish...
        dirs.WriteDummyData();
        dirs.DoWorkInSourceDirectory();
        backup_set_keep_capturing(false);
    } else {
        dirs.WriteDummyData();
        start_backup_thread_with_exclusion_callback(&thread, exclude_all, NULL);
        dirs.DoWorkInSourceDirectory();
    }

    finish_backup_thread(thread);
    result = dirs.CheckWork();
    dirs.Cleanup();
    return result;
}

int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    int r = 0;
    bool keep_capture = false;
    const int MAX_DIRS = 8;
    for (int i = 1; i <= MAX_DIRS; i = i << 1, keep_capture = !keep_capture)
    {
        // Alternate focusing on copying vs. capturing for each larger
        // set of directories.
        r = many_directories(i, keep_capture);
        if (r != 0)
        {
            break;
        }

        printf("PASS: with %d directories backed up.\n", i);
    }

    return r; 
}
