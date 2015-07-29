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

// Test to make sure nothing crashes if the backup destination directory has unwriteable permissions.

#include <errno.h>

#include "backup_test_helpers.h"

volatile bool saw_error = false;

static void expect_eacces_error_fun(int error_number, const char *error_string, void *backup_extra __attribute__((unused))) {
    fprintf(stderr, "EXPECT ERROR %d: %d (%s)\n", EACCES, error_number, error_string);
    check(error_number==EACCES);
    saw_error = true;
}

int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    setup_source();
    setup_destination();
    setup_dirs();
    char *src = get_src();
    char *dst = get_dst();
    {
        int r = systemf("chmod ugo-w %s", dst);
        check(r==0);
    }
    {
        const char *srcs[1] = {src};
        const char *dsts[1] = {dst};
        int r = tokubackup_create_backup(srcs, dsts, 1,
                                         simple_poll_fun, NULL,
                                         expect_eacces_error_fun, NULL,
                                         NULL, NULL);
        check(r==EACCES);
        check(saw_error);
    }
    cleanup_dirs();
    free(src);
    free(dst);
    return 0;
}

    
