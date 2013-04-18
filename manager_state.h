/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

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
    bool copy_is_enabled(void);
protected:
    void enable_capture(void);
    void disable_capture(void);
    void enable_copy(void);
    void disable_copy(void);
private:
    volatile bool m_is_dead;
    volatile bool m_capture_enabled;
    volatile bool m_copy_enabled;
};

#endif // End of header guardian.
