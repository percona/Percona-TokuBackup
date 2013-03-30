/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <valgrind/helgrind.h>

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
    assert(r<(int)sizeof(string));
    return system(string);
}

int openf(int flags, int mode, const char *formatstring, ...) {
    va_list ap;
    va_start(ap, formatstring);
    char *string  = (char*)malloc(PATH_MAX);
    {
        int r = vsnprintf(string, PATH_MAX, formatstring, ap);
        assert(r<=PATH_MAX);
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

struct backup_thread_extra_t {
    char *             src_dir;
    char *             dst_dir;
    backup_poll_fun_t  poll_fun;
    void*              poll_extra;
    backup_error_fun_t error_fun;
    void*              error_extra;
    int                expect_return_result;
};
static void* start_backup_thread_fun(void *backup_extra_v) {
    backup_thread_extra_t *backup_extra = (backup_thread_extra_t*)backup_extra_v;
    const char *srcs[1] = {backup_extra->src_dir};
    const char *dsts[1] = {backup_extra->dst_dir};
    int r = tokubackup_create_backup(srcs, dsts, 1,
                                     backup_extra->poll_fun,  backup_extra->poll_extra,
                                     backup_extra->error_fun, backup_extra->error_extra);
    if (r!=backup_extra->expect_return_result) {
        printf("%s:%d Got error %d, Expected %d\n", __FILE__, __LINE__, r, backup_extra->expect_return_result);
    }
    assert(r==backup_extra->expect_return_result);
    if (backup_extra->src_dir) free(backup_extra->src_dir);
    if (backup_extra->dst_dir) free(backup_extra->dst_dir);
    return backup_extra_v;
}

void start_backup_thread_with_funs(pthread_t *thread,
                                   char *src_dir, char *dst_dir,
                                   backup_poll_fun_t poll_fun, void *poll_extra,
                                   backup_error_fun_t error_fun, void *error_extra,
                                   int expect_return_result) {
    backup_thread_extra_t *p = new backup_thread_extra_t;
    //    *p = {src_dir, dst_dir, poll_fun, poll_extra, error_fun, error_extra, expect_return_result};
    p->src_dir = src_dir;
    p->dst_dir = dst_dir;
    p->poll_fun = poll_fun;
    p->poll_extra = poll_extra;
    p->error_fun = error_fun;
    p->error_extra = error_extra;
    p->expect_return_result = expect_return_result;
    int r = pthread_create(thread, NULL, start_backup_thread_fun, p);
    assert(r==0);
}

int simple_poll_fun(float progress, const char *progress_string, void *poll_extra) {
    assert(progress>=0 && progress <= 1);
    assert(progress_string!=NULL);
    assert(poll_extra==NULL);
    return 0;
}

static void expect_no_error_fun(int error_number, const char *error_string, void *backup_extra __attribute__((__unused__))) {
    fprintf(stderr, "Error, I expected no error function to run, but received error #%d: %s\n", error_number, error_string);
    abort();
}

void start_backup_thread(pthread_t *thread) {
    char *src = get_src();
    char *dst = get_dst();
    start_backup_thread_with_funs(thread, src, dst, simple_poll_fun, NULL, expect_no_error_fun, NULL, BACKUP_SUCCESS);
}

void start_backup_thread(pthread_t *thread, char* destination) {
    char *src = get_src();
    start_backup_thread_with_funs(thread, src, destination, simple_poll_fun, NULL, expect_no_error_fun, NULL, BACKUP_SUCCESS);
}

void finish_backup_thread(pthread_t thread) {
    void *retval;
    int r = pthread_join(thread, &retval);
    assert(r==0);
    assert(retval!=NULL);
    backup_thread_extra_t *extra = static_cast<backup_thread_extra_t*>(retval);
    delete(extra);
}

static const char *test_name = NULL;

char *get_dst(void) {
    assert(test_name);
    size_t size = strlen(test_name)+100;
    char s[size];
    int r = snprintf(s, sizeof(s), "%s.backup", test_name);
    assert(r<(int)size);
    char *result = strdup(s);
    assert(result);
    return result;
}

char *get_src(void) {
    assert(test_name);
    size_t size = strlen(test_name)+100;
    char s[size];
    int r = snprintf(s, size, "%s.source", test_name);
    assert(r<(int)size);
    char *result = strdup(s);
    assert(result);
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
            assert(argnum+1 < argc);
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
