/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id: manager_state.cc 55281 2013-04-09 21:09:36Z christianrober $"

#include <pthread.h>

#include <valgrind/helgrind.h>

#include "manager_state.h"

pthread_rwlock_t manager_state::m_capture_rwlock = PTHREAD_RWLOCK_INITIALIZER;

manager_state::manager_state()
    : m_is_dead(false),
      m_capture_enabled(false)
{
    VALGRIND_HG_DISABLE_CHECKING(&m_is_dead, sizeof(m_is_dead));
}

bool manager_state::is_dead(void)
{
    return m_is_dead;
}

bool manager_state::is_alive(void)
{
    return !m_is_dead;
}

void manager_state::kill(void)
{
    m_is_dead = true;
}

bool manager_state::capture_is_enabled(void)
{
    return m_capture_enabled;
}

int manager_state::capture_read_lock(void)
{
    return pthread_rwlock_rdlock(&m_capture_rwlock);
}

int manager_state::capture_unlock(void)
{
    return pthread_rwlock_unlock(&m_capture_rwlock);
}

int manager_state::enable_capture(void) {
    int r = pthread_rwlock_wrlock(&m_capture_rwlock);
    if (r != 0) {
        goto error_out;
    }

    m_capture_enabled = true;
    r = pthread_rwlock_unlock(&m_capture_rwlock);
    if (r != 0) {
        goto error_out;
    }
    
 error_out:
    return r;
}

int manager_state::disable_capture(void) 
{
    int r = pthread_rwlock_wrlock(&m_capture_rwlock);
    if (r != 0) {
        goto error_out;
    }

    m_capture_enabled = false;
    r = pthread_rwlock_unlock(&m_capture_rwlock);
    if (r != 0) {
        goto error_out;
    }
    
 error_out:
    return r;
}
