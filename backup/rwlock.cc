/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include <pthread.h>
#include "check.h"
#include "manager.h"
#include "mutex.h"
#include "rwlock.h"

void prwlock_rdlock(pthread_rwlock_t *lock, const backtrace bt) throw() {
    int r = pthread_rwlock_rdlock(lock);
    check_bt(r==0, bt);
}

void prwlock_wrlock(pthread_rwlock_t *lock, const backtrace bt) throw() {
    int r = pthread_rwlock_wrlock(lock);
    check_bt(r==0, bt);
}

void prwlock_unlock(pthread_rwlock_t *lock, const backtrace bt) throw() {
    int r = pthread_rwlock_unlock(lock);
    check_bt(r==0, bt);
}

void prwlock_rdlock(pthread_rwlock_t *lock) throw() {
    int r = pthread_rwlock_rdlock(lock);
    check(r==0);
}

void prwlock_wrlock(pthread_rwlock_t *lock) throw() {
    int r = pthread_rwlock_wrlock(lock);
    check(r==0);
}

void prwlock_unlock(pthread_rwlock_t *lock) throw() {
    int r = pthread_rwlock_unlock(lock);
    check(r==0);
}
