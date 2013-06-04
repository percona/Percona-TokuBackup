/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
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

int my_rename(const char *a, const char *b) {
    printf("Doing rename\n");
    int r = real_rename(a, b);
    check_fun(0, "call check fun to make sure it fails properly", BACKTRACE(NULL));
    return r;
}

char *A, *B, *C, *D;

void handler(int i __attribute__((unused))) {
    int r = rename(C, D);
    if (r!=0) raise(SIGKILL);
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
