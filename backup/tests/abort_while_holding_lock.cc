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

// We want to make sure that the check() macro doesn't deadlock if
// it's called while holding one of the backup locks, even if while
// running abort(), or some signal handler that abort might call, an
// I/O happens that might try to grab that lock if the backup system
// is running.  This is covered in git issue #24.

#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "backtrace.h"
#include "backup_test_helpers.h"
#include "real_syscalls.h"

// Get rid of the check in backup_test_helpers, in favor of the one that the rest of the backup library uses.
#undef check
#include "../check.h"

rename_fun_t real_rename = NULL;

__thread int stack_depth = 0;

int my_rename(const char *a, const char *b) {
    printf("Doing rename\n");
    int r = real_rename(a, b);
    stack_depth++;
    if (stack_depth==1) {
        check_fun(0, "call check fun to make sure it fails properly", BACKTRACE(NULL));
    }
    stack_depth--;
    return r;
}

char *A, *B, *C, *D;

void handler(int i __attribute__((unused))) {
    int r = rename(C, D);
    if (0) printf("%s:%d r=%d\n", __FILE__, __LINE__, r);
    exit(0); // want to exit gracefully.
}

int test_main(int argc __attribute__((unused)), const char *argv[] __attribute__((unused))) {
    signal(SIGABRT, handler);
    real_rename = register_rename(my_rename);
    setup_source();
    char *src = get_src();
    size_t len = strlen(src)+100;
    A = (char*)malloc(len); { int r = snprintf(A, len, "%s/A", src); assert(size_t(r)<len); }
    B = (char*)malloc(len); { int r = snprintf(B, len, "%s/B", src); assert(size_t(r)<len); }
    C = (char*)malloc(len); { int r = snprintf(A, len, "%s/C", src); assert(size_t(r)<len); }
    D = (char*)malloc(len); { int r = snprintf(B, len, "%s/D", src); assert(size_t(r)<len); }
    {
        int fd = open(A, O_WRONLY | O_CREAT, 0777);
        assert(fd>=0);
        int r = close(fd);
        assert(r==0);
    }
    {
        int r = rename(A, B);
        assert(r==0);
    }
    cleanup_dirs();
    free(src);
    free(A);
    free(B);
    return 0;
}
