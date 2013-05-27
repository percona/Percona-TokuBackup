/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id: fmap.cc 55798 2013-04-20 12:33:47Z christianrober $"

#include "soft_barrier.h"
#include "check.h"

pthread_mutex_t soft_barrier::mutex        = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  soft_barrier::cond         = PTHREAD_COND_INITIALIZER;
bool            soft_barrier::capture_mode = false;
uint64_t        soft_barrier::counts[2]    = {0,0};

static void plock(pthread_mutex_t *m) {
    int r = pthread_mutex_lock(m);
    check(r==0);
}

static void punlock(pthread_mutex_t *m) {
    int r = pthread_mutex_unlock(m);
    check(r==0);
}

static void pwait(pthread_cond_t *c, pthread_mutex_t *m) {
    int r = pthread_cond_wait(c, m);
    check(r==0);
}

static void psignal(pthread_cond_t *c) {
    int r = pthread_cond_signal(c);
    check(r==0);
}

void soft_barrier::set_capture_mode_and_wait(void) {
    plock(&mutex);
    check(!capture_mode);
    capture_mode = true;
    while (counts[0]>0) {
        pwait(&cond, &mutex);
    }
    punlock(&mutex);
}

void soft_barrier::clear_capture_mode_and_wait(void) {
    plock(&mutex);
    check(capture_mode);
    capture_mode = false;
    while (counts[1]>0) {
        pwait(&cond, &mutex);
    }
    punlock(&mutex);
}

bool soft_barrier::enter_operation(void) {
    plock(&mutex);
    bool result = capture_mode;
    counts[result ? 1 : 0]++;
    punlock(&mutex);
    return result;
}

bool soft_barrier::enter_operation(barrier_location *bloc) {
    bloc->increment(1);
    return enter_operation();
}

void soft_barrier::finish_operation(bool mode) {
    plock(&mutex);
    int idx = mode ? 1 : 0;
    check(counts[idx]>0);
    counts[idx]--;
    if (counts[idx]==0) {
        psignal(&cond);
    }
    punlock(&mutex);
}
    

void soft_barrier::finish_operation(bool mode, barrier_location *bloc) {
    bloc->increment(-1);
    return finish_operation(mode);
}
