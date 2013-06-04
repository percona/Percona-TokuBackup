/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#ifndef MUTEX_H
#define MUTEX_H

#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include <pthread.h>
#include "backtrace.h"

#ifndef NO_DEPRECATE_PTHREAD_MUTEXES
extern int pthread_mutex_lock(pthread_mutex_t *) __attribute__((deprecated));
extern int pthread_mutex_unlock(pthread_mutex_t *) __attribute__((deprecated));
#endif

// We assume there are no errors returned by these functions (if mutexes are broken, then other things are going wrong...)  This function checks for errors and aborts.
extern void pmutex_lock(pthread_mutex_t *) throw();
extern void pmutex_unlock(pthread_mutex_t *) throw();

extern void pmutex_lock(pthread_mutex_t *, const backtrace) throw();
extern void pmutex_unlock(pthread_mutex_t *, const backtrace) throw();

class with_mutex_locked {
  private:
    pthread_mutex_t *m_mutex;
    bool m_have_backtrace;
    const backtrace m_backtrace;
  public:
    with_mutex_locked(pthread_mutex_t *m): m_mutex(m), m_have_backtrace(false) {
        pmutex_lock(m_mutex);
    }
    with_mutex_locked(pthread_mutex_t *m, const backtrace bt): m_mutex(m), m_have_backtrace(true), m_backtrace(bt) {
        pmutex_lock(m_mutex, m_backtrace);
    }
    ~with_mutex_locked(void) {
        if (m_have_backtrace) {
            pmutex_unlock(m_mutex, m_backtrace);
        } else {
            pmutex_unlock(m_mutex);
        }
    }
};

#endif
