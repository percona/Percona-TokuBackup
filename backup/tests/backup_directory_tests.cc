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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

#include "backup_test_helpers.h"
#include "backup_internal.h"
#include "backup_directory.h"

#define LONG_DIR "/ThisIsALongDirectory/WithNothing/InIt/"

static int backup_sub_dirs(void) {
    setup_destination();
    setup_source();

    char *src = get_src();
    char *dst = get_dst();
    char *source = realpath(src, NULL);
    char *destination = realpath(dst, NULL);

    // We need to pretend that there is a source directory with this
    // long path.  So, let's first create enough space for it.
    char *newpath = (char*)malloc(PATH_MAX);
    strcpy(newpath, destination);
    char *temp = newpath;

    // Move to the end of the absolute destination path we just
    // copied.
    while(*temp++) {;}
    --temp;

    // Append the long directory path to the destination path we just
    // copied.
    const char *longname = LONG_DIR;
    while(*longname) {
        *temp = *(longname)++;
        temp++;
    }

    *temp = 0;
    {
        int r = create_subdirectories(newpath);
        check(r==0);
    }
    free(newpath);

    // Verify:
    struct stat sb;
    char dst_long_dir[PATH_MAX];
    {
        int r = snprintf(dst_long_dir, sizeof(dst_long_dir), "%s%s", dst, LONG_DIR);
        check(r<PATH_MAX);
    }
    int r = stat(dst_long_dir, &sb);

    if(source) free(source);
    if(destination) free(destination);

    free(src);
    free(dst);

    if (r) {
        fail();
    } else {
        pass();
        cleanup_dirs();
    }

    printf(": backup_sub_dirs()\n");

    return r;
}

int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    int r = backup_sub_dirs();
    return r;
}
