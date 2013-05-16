/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id: realpath_injection.cc 55458 2013-04-13 21:44:50Z christianrober $"

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "realpath_library.h"
#include "backup_test_helpers.h"

static int expect_error = REALPATH::ERROR_TO_INJECT;
static pthread_t backup_thread;

static void my_error_fun(int e, const char *s, void *ignore) 
{
    check(ignore==NULL);
    fprintf(stderr, "Got error %d (I expected errno=%d) (%s)\n", e, expect_error, s);
}

void start_backup(void)
{
    backup_set_keep_capturing(true);
    start_backup_thread_with_funs(&backup_thread,
                                  get_src(),
                                  get_dst(),
                                  simple_poll_fun,
                                  NULL,
                                  my_error_fun,
                                  NULL,
                                  expect_error);
    sleep(1);
    while (!backup_done_copying()) { sched_yield(); }
}

void finish_backup(void)
{
    backup_set_keep_capturing(false);
    finish_backup_thread(backup_thread);
}

const mode_t CREATE_FLAGS = 0777;

int call_create(const char * const file)
{
    int fd = open(file, O_CREAT | O_RDWR, CREATE_FLAGS);
    return fd;
}

int call_open(const char * const file)
{
    int fd = open(file, O_RDWR);
    return fd;
}

void call_rename(const char * const old_file, const char * const new_file)
{
    int r = rename(old_file, new_file);
    if (r != 0) {
        
    }
}

void call_unlink(const char * const file)
{
    int r = unlink(file);
    if (r != 0) {
        
    }
}

int test_main(int n, const char **p)
{
    n++;
    p++;
    setup_source();
    setup_destination();

    int fd = 0;
    char file[PATH_MAX] = {0};
    char * src = get_src();
    snprintf(file, PATH_MAX, "%s/%s", src, "foo.txt");
    const char * dummy = realpath(file, NULL);
    if (dummy == NULL) {;}

    // 1. create
    printf("create...\n");
    start_backup();
    REALPATH::inject_error(true);
    fd = call_create(file);
    REALPATH::inject_error(false);
    close(fd);
    finish_backup();

    // 2. open
    start_backup();
    printf("open...\n");
    REALPATH::inject_error(true);
    fd = call_open(file);
    REALPATH::inject_error(false);
    finish_backup();
    close(fd);

    // 3. rename
    printf("rename...\n");
    setup_destination();
    start_backup();
    REALPATH::inject_error(true);
    call_rename(file, file);
    REALPATH::inject_error(false);
    finish_backup();

    // 4. unlink
    printf("unlink...\n");
    setup_destination();
    start_backup();
    REALPATH::inject_error(true);
    call_unlink(file);
    REALPATH::inject_error(false);
    finish_backup();

    cleanup_dirs();
    free(src);
    return 0;
}
