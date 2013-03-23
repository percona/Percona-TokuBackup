/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "backup_test_helpers.h"
#include "backup.h"

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
    char string[PATH_MAX];
    int r = vsnprintf(string, sizeof(string), formatstring, ap);
    assert(r<=(int)sizeof(string));
    return open(string, flags, mode);
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
    *p = {src_dir, dst_dir, poll_fun, poll_extra, error_fun, error_extra, expect_return_result};
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


void finish_backup_thread(pthread_t thread) {
    void *retval;
    int r = pthread_join(thread, &retval);
    assert(r==0);
    assert(retval!=NULL);
    free(retval);
}

char *get_dst(void) {
    size_t size = strlen(BACKUP_NAME)+100;
    char s[size];
    int r = snprintf(s, sizeof(s), "%s.backup", BACKUP_NAME);
    assert(r<(int)size);
    char *result = strdup(s);
    assert(result);
    return result;
}

char *get_src(void) {
    size_t size = strlen(BACKUP_NAME)+100;
    char s[size];
    int r = snprintf(s, size, "%s.backup", BACKUP_NAME);
    assert(r<(int)size);
    char *result = strdup(s);
    assert(result);
    return result;
}
