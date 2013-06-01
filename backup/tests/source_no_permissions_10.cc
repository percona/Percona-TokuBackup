/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

// Test to make sure nothing crashes if the backup source directory has unreadable permissions.

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
        int r = systemf("chmod ugo-w %s", src);
        check(r==0);
    }
    {
        const char *srcs[1] = {src};
        const char *dsts[1] = {dst};
        int r = tokubackup_create_backup(srcs, dsts, 1,
                                         simple_poll_fun, NULL,
                                         expect_eacces_error_fun, NULL);
        check(r==EACCES);
        check(saw_error);
    }
    cleanup_dirs();
    free(src);
    free(dst);
    return 0;
}

    
