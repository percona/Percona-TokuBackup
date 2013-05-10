/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ifndef MANAGER_STATE_H
#define MANAGER_STATE_H

#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include <pthread.h>

class manager_state {
public:
    manager_state() throw();
    bool is_dead(void) throw();
    bool is_alive(void) throw();
    void kill(void) throw();
    bool capture_is_enabled(void) throw();
    bool copy_is_enabled(void) throw();
protected:
    void enable_capture(void) throw();
    void disable_capture(void) throw();
    void enable_copy(void) throw();
    void disable_copy(void) throw();
private:
    volatile bool m_is_dead;
    volatile bool m_capture_enabled;
    volatile bool m_copy_enabled;
};

#endif // End of header guardian.
