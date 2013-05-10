/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id: multiple_backups.cc 55852 2013-04-23 02:07:52Z christianrober $"

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

static char *src, *dst;
__thread int64_t delay_me_count = -1;

static rename_fun_t original_rename;
static int my_rename(const char *oldpath, const char *newpath) {
    printf("Rename\n %s\n %s\n", oldpath, newpath);
    printf("delay_me_count=%ld\n", delay_me_count);
    //    printf("my_open(\"%s\", %d,...) delay=%d\n", path, flags, delay_it);
    //    if (delay_it) {
    //        backup_set_keep_capturing(false); // inside the open we let the capture finish, then sleep
    //        sleep(1);
    //        printf("slept.\n");
    //    }
    if (delay_me_count--==0) {
        backup_set_keep_capturing(false);
        sleep(1);
    }
    int r = original_rename(oldpath, newpath);
    if (delay_me_count--==0) {
        backup_set_keep_capturing(false);
        sleep(1);
    }
    return r;
}

int64_t do_rename_delay_count = -1;
static void* do_rename(void* ignore) {
    delay_me_count = do_rename_delay_count;
    {
        int fd = openf(O_RDWR | O_CREAT, 0777, "%s/old.data", src);
        check(fd>=0);
        int r = close(fd);
        check(r==0);
    }

    size_t len = strlen(src) + 10; 
    char *gonpath = (char*)malloc(len);
    char *newpath = (char*)malloc(len);
    {
        size_t r = snprintf(gonpath, len, "%s/gon.data", src);
        check(r<len);
    }
    {
        size_t r = snprintf(newpath, len, "%s/new.data", src);
        check(r<len);
    }

    backup_set_keep_capturing(true); //this must be done before start_copying.
    backup_set_start_copying(true);
    while(!backup_done_copying()) sched_yield();
    int r = rename(gonpath, newpath);
    check(r==-1);
    backup_set_keep_capturing(false);
    free(gonpath);
    free(newpath);
    return ignore;
}

int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    src = get_src();
    dst = get_dst();
    setup_source();
    setup_destination();
    original_rename = register_rename(my_rename);
    pthread_t thread;
    pthread_t open_th;
    backup_set_start_copying(false);
    start_backup_thread(&thread);
    {
        do_rename_delay_count = 0;
        int r = pthread_create(&open_th, NULL, do_rename, NULL);
        check(r==0);
    }
    finish_backup_thread(thread);
    {
        void *result;
        int r = pthread_join(open_th, &result);
        check(r==0 && result==NULL);
    }
    free(src);
    free(dst);
    return 0;
}
