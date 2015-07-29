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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "backup_test_helpers.h"
#include "copier.h"
#include "file_hash_table.h"

const int TEXT_COUNT = 9;
const char *text[TEXT_COUNT] = {"oh", "well", "hello", "there", "kitty", "cat", "how", "are", "you"};
const int FILES_PER_DIR = 3;
const int FILE_COUNT = 3;
const char *files[FILE_COUNT] = {"/first.data", "/second.data", "/reallyLongFileNameThatShouldNotBreak.data"};
const int DIR_COUNT = 3;
const char *dirs[DIR_COUNT] = {"/subdir", "/mydir", "/reallyLongSubDirectoryName"};

const char *BACKUP_NAME = __FILE__;

char *src, *dst;

static int verify_large_dir(void) {
    int r = 0;
    char str[PATH_MAX];
    struct stat sb;

    // 1. Verify that the three files are at the top.
    for(int i = 0; i < FILE_COUNT; ++i) {
        strcpy(str, dst);
        strcat(str, files[i]);
        r = stat(str, &sb);
        if(r) {
            perror(str);
            return r;
        }
    }
   
    // 2. Verify the contents of each file are correct.
    // TODO:

    // 3. Verify there are two subdirectories.
    for(int i = 0; i < DIR_COUNT -1; ++i) {
        strcpy(str, dst);
        strcat(str, dirs[i]);
        r = stat(str, &sb);
        if (r) {
            perror(str);
            return r;
        }
    }

    // 4. Verify contents of each file in first subdir.
    // TODO:
    for(int i = 0; i < FILE_COUNT; ++i) {
        strcpy(str, dst);
        strcat(str, dirs[0]);
        strcat(str, files[i]);
        r = stat(str, &sb);
        if(r) {
            perror(str);
            return r;
        }
    }

    // 5. Verify there is one more subdirectory inside 2nd initial subdir.
    strcpy(str, dst);
    strcat(str, dirs[1]);
    strcat(str, dirs[2]);
    r = stat(str, &sb);
    if(r) {
        perror(str);
        return r;
    }

    // 6. Verify contents of each file in lowest dir. 
    // TODO:
    return r;
}


static void setup_large_dir(void) {
    // Create files in the top level directory.
    for (int i = 0; i < FILE_COUNT; ++i) {
        systemf("echo %s > %s%s", text[i], src, files[i]);
    }

    // Make two sub dirs.
    for(int i = 0; i < DIR_COUNT - 1; ++i) {
        systemf("mkdir %s%s", src, dirs[i]);
    }

    // Populate the first sub dir.
    for (int i = 0; i < FILE_COUNT; ++i) {
        systemf("echo %s > %s%s%s", text[i + FILES_PER_DIR], src, dirs[0], files[i]);
    }

    // Create the deeper sub dir.
    systemf("mkdir %s%s%s", src, dirs[1], dirs[2]);

    // Populate the deeper sub dir.
    for (int i = 0; i < FILE_COUNT; ++i) {
        systemf("echo %s > %s%s%s%s", text[i + (FILES_PER_DIR * 2)], src, dirs[1], dirs[2], files[i]);
    }
/******
    system("echo " "oh > " SRC "/first.data");
    system("echo well > " SRC "/second.data");
    system("echo hello > " SRC "/reallyLongNameThatShouldNotBreak.data");
    system("mkdir " SRC "/subdir");
    system("echo there > " SRC "/subdir/sub.data");
    system("echo kitty > " SRC "/subdir/longNameThatShouldNotBreakPartTwo.data");
    system("mkdir " SRC "/mysql");
    system("mkdir " SRC "/mysql/reallylongsubdirectoryname");
    system("echo cat > " SRC "/mysql/reallylongsubdirectoryname/longNameThatShouldNotBreakPartTwo.data");
*****/
}

static void copy_files(void) {
    src = get_src();
    dst = get_dst();

    setup_source();
    setup_destination();
    setup_large_dir();

    backup_callbacks calls(&dummy_poll, NULL, &dummy_error, NULL, NULL, NULL, &dummy_throttle);
    file_hash_table table;
    copier the_copier(&calls, &table);
    the_copier.set_directories(src, dst);
    {
        int r = the_copier.do_copy();
        check(r==0);
    }

    int r = verify_large_dir();
    if(r) {
        fail();
    } else {
        pass();
    }

    cleanup_dirs();
    free(src);
    free(dst);
    printf(": copy_files()\n");
}

int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    copy_files();
    return 0;
}
