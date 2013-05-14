/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

/* Create a race at the end */

#include "backup_internal.h"
#include "backup_test_helpers.h"
#include "real_syscalls.h"

#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    char *src = get_src();
    char *dst = get_dst();
    setup_source();
    setup_destination();

    size_t len = strlen(src) + 20; 
    char *no_such_path = (char*)malloc(len);
    {
        size_t r = snprintf(no_such_path, len, "%s/no_such.data", src);
        check(r<len);
    }

    pthread_t thread;
    backup_set_start_copying(false);
    start_backup_thread(&thread);
    backup_set_keep_capturing(true);
    backup_set_start_copying(true);
    int r = unlink(no_such_path);
    check(r==-1);
    backup_set_keep_capturing(false);
    finish_backup_thread(thread);
    free(no_such_path);
    free(src);
    free(dst);
    return 0;
}
