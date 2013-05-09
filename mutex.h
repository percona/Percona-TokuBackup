/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#ifndef MUTEX_H
#define MUTEX_H

#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id: fmap.h 55798 2013-04-20 12:33:47Z christianrober $"

#include <pthread.h>

#ifndef NO_DEPRECATE_PTHREAD_MUTEXES
extern int pthread_mutex_lock(pthread_mutex_t *) __attribute__((deprecated));
extern int pthread_mutex_unlock(pthread_mutex_t *) __attribute__((deprecated));
#endif

// We assume there are no errors returned by these functions (if mutexes are broken, then other things are going wrong...)  This function checks for errors and aborts.
extern void pmutex_lock(pthread_mutex_t *);
extern void pmutex_unlock(pthread_mutex_t *);

// these will go away
extern int pmutex_lock_c(pthread_mutex_t *) __attribute__((__warn_unused_result__));
extern int pmutex_unlock_c(pthread_mutex_t *) __attribute__((__warn_unused_result__));

#endif
