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

#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "backup_helgrind.h"

#include "backup_test_helpers.h"
#include "backup_internal.h"
#include "backup_callbacks.h"

const char * const DEFAULT_TERM = "\033[0m";
const char * const RED_TERM = "\033[31m";
const char * const GREEN_TERM = "\033[32m";

void pass(void) {
    printf(GREEN_TERM);
    printf("[PASS]");
    printf(DEFAULT_TERM);
}

void fail(void) {
    printf(RED_TERM);
    printf("[FAIL]");
    printf(DEFAULT_TERM);
}

int systemf(const char *formatstring, ...) {
    va_list ap;
    va_start(ap, formatstring);
    char string[1000];
    int r = vsnprintf(string, sizeof(string), formatstring, ap);
    va_end(ap);
    check(r<(int)sizeof(string));
    return system(string);
}

int openf(int flags, int mode, const char *formatstring, ...) {
    va_list ap;
    va_start(ap, formatstring);
    char *string  = (char*)malloc(PATH_MAX);
    {
        int r = vsnprintf(string, PATH_MAX, formatstring, ap);
        check(r<=PATH_MAX);
    }
    int r = open(string, flags, mode);
    free(string);
    return r;
}

void setup_destination(void) {
    char *dst = get_dst();
    systemf("rm -rf %s", dst);
    systemf("mkdir %s",  dst);
    free(dst);
}

void setup_source(void) {
    char *src = get_src();
    systemf("rm -rf %s", src);
    systemf("mkdir %s",  src);
    free(src);
}

void setup_directory(char *dir) {
    systemf("rm -rf %s", dir);
    systemf("mkdir %s", dir);
}

void setup_dirs(void) {
    char *src = get_src();
    systemf("touch %s/foo", src);
    systemf("echo hello > %s/bar.data", src);
    systemf("mkdir %s/subdir", src);
    systemf("echo there > %s/subdir/sub.data", src);
    free(src);
}

void cleanup_dirs(void) {
    {
        char *dst = get_dst();
        systemf("rm -rf %s", dst);
        free(dst);
    }
    {
        char *src = get_src();
        systemf("rm -rf %s", src);
        free(src);
    }
}

// Statics and constants for adding mult-directory support.
static int dir_count = 1;
const int MAX_DIR_COUNT = 16;
const char *sources[MAX_DIR_COUNT];
const char *destinations[MAX_DIR_COUNT];

void set_dir_count(int new_count) {
    if (new_count >= 1 && new_count <= 16) {
        dir_count = new_count;
    }
}

struct backup_thread_extra_t {
    const char **            src_dirs;
    const char **            dst_dirs;
    backup_poll_fun_t  poll_fun;
    void*              poll_extra;
    backup_error_fun_t error_fun;
    void*              error_extra;
    backup_exclude_copy_fun_t exclude_fun;
    void*                     exclude_extra;
    int                expect_return_result;
};

static volatile bool backup_is_done = false;

static void* start_backup_thread_fun(void *backup_extra_v) {
    backup_thread_extra_t *backup_extra = (backup_thread_extra_t*)backup_extra_v;
    const char **srcs = {backup_extra->src_dirs};
    const char **dsts = {backup_extra->dst_dirs};
    int r = tokubackup_create_backup(srcs, dsts, dir_count,
                                     backup_extra->poll_fun,  backup_extra->poll_extra,
                                     backup_extra->error_fun, backup_extra->error_extra,
                                     backup_extra->exclude_fun, backup_extra->exclude_extra);
    if (r!=backup_extra->expect_return_result) {
        printf("%s:%d Got error %d, Expected %d\n", __FILE__, __LINE__, r, backup_extra->expect_return_result);
    }
    check(r==backup_extra->expect_return_result);
    for (int i = 0; i < dir_count; ++i) {
        if (backup_extra->src_dirs[i]) free((void *)backup_extra->src_dirs[i]);
        if (backup_extra->dst_dirs[i]) free((void*)backup_extra->dst_dirs[i]);
    }
    backup_is_done = true;
    return backup_extra_v;
}

bool backup_thread_is_done(void) {
    return backup_is_done;
}

void start_backup_thread_with_funs(pthread_t *thread,
                                   char *src, char *dst,
                                   backup_poll_fun_t poll_fun, void *poll_extra,
                                   backup_error_fun_t error_fun, void *error_extra,
                                   int expect_return_result) {
    set_dir_count(1);
    sources[0] = src;
    destinations[0] = dst;
    start_backup_thread_with_funs(thread, sources, destinations, 
                                  poll_fun, poll_extra,
                                  error_fun, error_extra,
                                  expect_return_result);
}

void start_backup_thread_with_error_poll_and_exclude_funs(pthread_t *thread,
                                                          const char *src_dirs[],
                                                          const char *dst_dirs[],
                                                          backup_poll_fun_t poll_fun,
                                                          void *poll_extra,
                                                          backup_error_fun_t error_fun,
                                                          void *error_extra,
                                                          backup_exclude_copy_fun_t exclude_fun,
                                                          void *exclude_extra,
                                                          int expect_return_result) {
    backup_thread_extra_t *p = new backup_thread_extra_t;
    //    *p = {src_dir, dst_dir, poll_fun, poll_extra, error_fun, error_extra, expect_return_result};
    p->src_dirs = src_dirs;
    p->dst_dirs = dst_dirs;
    p->poll_fun = poll_fun;
    p->poll_extra = poll_extra;
    p->error_fun = error_fun;
    p->error_extra = error_extra;
    p->expect_return_result = expect_return_result;
    TOKUBACKUP_VALGRIND_HG_DISABLE_CHECKING(&backup_is_done, sizeof(backup_is_done));
    p->exclude_fun = exclude_fun;
    p->exclude_extra = exclude_extra;
    backup_is_done = false;
    int r = pthread_create(thread, NULL, start_backup_thread_fun, p);
    check(r==0);
}
void start_backup_thread_with_funs(pthread_t *thread,
                                   const char *src_dirs[], const char *dst_dirs[],
                                   backup_poll_fun_t poll_fun, void *poll_extra,
                                   backup_error_fun_t error_fun, void *error_extra,
                                   int expect_return_result) {
    start_backup_thread_with_error_poll_and_exclude_funs(thread,
                                                         src_dirs,
                                                         dst_dirs,
                                                         poll_fun,
                                                         poll_extra,
                                                         error_fun,
                                                         error_extra,
                                                         NULL,
                                                         NULL,
                                                         expect_return_result);
}

int simple_poll_fun(float progress, const char *progress_string, void *poll_extra) {
    printf("progress=%f\n", progress);
    check(progress>=0);
    if (0) check(progress <= 1); // This is no longer true.  When files are deleted, the progress could > 1.
    check(progress_string!=NULL);
    check(poll_extra==NULL);
    return 0;
}

static void expect_no_error_fun(int error_number, const char *error_string, void *backup_extra __attribute__((__unused__))) {
    fprintf(stderr, "Error, I expected no error function to run, but received error #%d: %s\n", error_number, error_string);
    abort();
}

void start_backup_thread_with_exclusion_callback(pthread_t *thread, backup_exclude_copy_fun_t fun, void* extra) {
    for (int i = 0; i < dir_count; ++i) {
        sources[i] = get_src(i);
        destinations[i] = get_dst(i);
    }

    start_backup_thread_with_error_poll_and_exclude_funs(thread,
                                                         sources,
                                                         destinations,
                                                         simple_poll_fun,
                                                         NULL,
                                                         expect_no_error_fun,
                                                         NULL,
                                                         fun,
                                                         extra,
                                                         BACKUP_SUCCESS);
}

void start_backup_thread(pthread_t *thread) {
    for (int i = 0; i < dir_count; ++i) {
        sources[i] = get_src(i);
        destinations[i] = get_dst(i);
    }

    start_backup_thread_with_funs(thread, sources, destinations, simple_poll_fun, NULL, expect_no_error_fun, NULL, BACKUP_SUCCESS);
}

void start_backup_thread(pthread_t *thread, char* destination) {
    sources[0] = get_src();
    destinations[0] = destination;
    start_backup_thread_with_funs(thread, sources, destinations, simple_poll_fun, NULL, expect_no_error_fun, NULL, BACKUP_SUCCESS);
}

void finish_backup_thread(pthread_t thread) {
    void *retval;
    int r = pthread_join(thread, &retval);
    check(r==0);
    check(retval!=NULL);
    backup_thread_extra_t *extra = static_cast<backup_thread_extra_t*>(retval);
    delete(extra);
}

static const char *test_name = NULL;

char *get_dst(int dir_index) {
    check(test_name);
    size_t size = strlen(test_name)+100;
    char s[size];
    int r = snprintf(s, sizeof(s), "%s_%d.backup", test_name, dir_index);
    check(r<(int)size);
    char *result = strdup(s);
    check(result);
    return result;
}

char *get_src(int dir_index) {
    check(test_name);
    size_t size = strlen(test_name)+100;
    char s[size];
    int r = snprintf(s, size, "%s_%d.source", test_name, dir_index);
    check(r<(int)size);
    char *result = strdup(s);
    check(result);
    return result;
}

//////////////////////////////////////////////////////////////////
//
// Dummy callbacks:
//
int dummy_poll(float progress __attribute__((__unused__)), const char *string __attribute__((__unused__)), void *extra __attribute__((__unused__)))
{
    return 0;
}

void dummy_error(int error __attribute__((__unused__)), const char *string __attribute__((__unused__)), void *extra __attribute__((__unused__)))
{
}

unsigned long dummy_throttle(void)
{
    return ULONG_MAX;
}

///
int main (int argc, const char *argv[]) {
    const char *new_argv[argc];
    int  new_argc = 0;
    int  argnum   = 0;

    new_argv[new_argc++] = argv[argnum++];  // copy command

    while (argnum < argc) {
#define TESTNAME "--testname"
        if (0==strcmp(argv[argnum], TESTNAME)) {
            check(argnum+1 < argc);
            test_name = argv[argnum+1];
            argnum+=2;
        } else if (0==strncmp(argv[argnum], TESTNAME "=", sizeof(TESTNAME))) {
            test_name = argv[argnum]+sizeof(TESTNAME);
            argnum++;
        } else {
            new_argv[new_argc++] = argv[argnum];
        }
    }
    if (test_name==NULL) test_name=argv[0]; // make the function work with no arguments.
    return test_main(new_argc, new_argv);
}
