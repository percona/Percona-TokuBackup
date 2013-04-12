/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id: manager_state.h 55281 2013-04-09 21:09:36Z christianrober $"

#ifndef MANAGER_STATE_H
#define MANAGER_STATE_H

#include <pthread.h>

class manager_state {
public:
    manager_state();
    bool is_dead(void);
    bool is_alive(void);
    void kill(void);
    bool capture_is_enabled(void);
    int capture_read_lock(void);
    int capture_unlock(void);
protected:
    int enable_capture(void);
    int disable_capture(void);
private:
    volatile bool m_is_dead;
    volatile bool m_capture_enabled;
    static pthread_rwlock_t m_capture_rwlock;
};

#endif // End of header guardian.
