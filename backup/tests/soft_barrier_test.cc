/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id: backup_directory_tests.cc 55457 2013-04-13 21:39:13Z bkuszmaul $"

#include <pthread.h>
#include <unistd.h>

#include "backup_test_helpers.h"
#include "check.h"
#include "soft_barrier.h"

int n_out; // how many client threads are definitely out of capture mode.  There may be more.
int n_in;  // how many are definitely in capture mode.  There may be more.

void do_client_op(void) {
    usleep(1);
    bool b = soft_barrier::enter_operation();
    if (b) {
        __sync_fetch_and_add(&n_in, 1);
    } else {
        __sync_fetch_and_add(&n_out, 1);
    }
    usleep(1);
    if (b) {
        __sync_fetch_and_add(&n_in, -1);
    } else {
        __sync_fetch_and_add(&n_out, -1);
    }
    soft_barrier::finish_operation(b);
}

void do_server_op(void) {
    check(n_in==0);
    soft_barrier::set_capture_mode_and_wait();
    check(n_out==0);
    usleep(1);
    soft_barrier::clear_capture_mode_and_wait();
    check(n_in==0);
}

int n_undone=0;

void *do_client(void*v) {
    for (int i=0; i<10000; i++) {
        do_client_op();
    }
    __sync_fetch_and_add(&n_undone, -1);
    while (n_undone>0) {
        do_client_op();
    }
    return v;
}

int test_main(int argc __attribute__((unused)), const char *argv[]__attribute__((unused))) {
    const int N = 3;
    pthread_t threads[N];
    int id[N];
    n_undone = N+1;
    n_out=0;
    n_in =0;
    for (int i=0; i<N; i++) {
        id[i] = i;
        int r = pthread_create(&threads[i], NULL, do_client, &id[i]);
        check(r==0);
    }
    for (int i=0; i<100; i++) {
        do_server_op();
    }
    __sync_fetch_and_add(&n_undone, -1);
    while (n_undone>0) {
        do_server_op();
    }
    for (int i=0; i<N; i++) {
        void *v;
        int r = pthread_join(threads[i], &v);
        check(r==0);
        check(v==(void*)&id[i]);
    }
    return 0;
}
