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

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


#include "backup.h"
#include "backup_test_helpers.h"

// Test for #6317 dest dir is not a directory.
const int expect_result = EINVAL; // 
int   error_count=0;
bool  ok=true;

static void expect_error(int error_number, const char *error_string, void *error_extra) {
    if (error_count!=0)              { ok=false; fprintf(stderr, "%s:%d error function called twice\n", __FILE__, __LINE__); }
    error_count++;
    if (error_number!=expect_result) { ok=false; fprintf(stderr, "%s:%d error_number=%d expected %d\n", __FILE__, __LINE__, error_number, expect_result); }
    if (error_string==NULL)          { ok=false; fprintf(stderr, "%s:%d expect error_string nonnull\n", __FILE__, __LINE__); }
    printf("error_string (expected)=%s\n", error_string);
    if (error_extra!=NULL)           { ok=false; fprintf(stderr, "%s:%d expect error_extra NULL\n", __FILE__, __LINE__); }
}
    
struct dirent *readdir(DIR *dirp __attribute__((unused))) {
    // use this version of readdir, which won't work, as expecgted
    errno = EINVAL;
    return NULL;
}

int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    cleanup_dirs(); // remove destination dir
    setup_source();
    setup_destination();   // set up the dest
    setup_dirs();
    // Dest dir is a file, not a directory
    pthread_t thread;
    start_backup_thread_with_funs(&thread, get_src(), get_dst(), simple_poll_fun, NULL, expect_error, NULL, expect_result);
    finish_backup_thread(thread);
    if (ok && error_count!=1) {
        ok=false;
        fprintf(stderr, "%s:%d expect error_count==1 but it is %d\n", __FILE__, __LINE__, error_count);
    }
    cleanup_dirs();
    if (ok) {
        pass();
    } else {
        fail();
    }
    printf(": test6317()\n");
    return 0;
}
