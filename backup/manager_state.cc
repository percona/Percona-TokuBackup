/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include <pthread.h>

#include <valgrind/helgrind.h>

#include "atomics.h"
#include "manager_state.h"

manager_state::manager_state() throw()
    : m_is_dead(false),
      m_capture_enabled(false),
      m_copy_enabled(false)
{
    VALGRIND_HG_DISABLE_CHECKING(&m_is_dead,         sizeof(m_is_dead));
    VALGRIND_HG_DISABLE_CHECKING(&m_capture_enabled, sizeof(m_capture_enabled));
    VALGRIND_HG_DISABLE_CHECKING(&m_copy_enabled,    sizeof(m_copy_enabled));
}

bool manager_state::is_dead(void) throw() {
    return atomic_load_strong(&m_is_dead);
}

bool manager_state::is_alive(void) throw() {
    return !atomic_load_strong(&m_is_dead);
}

void manager_state::kill(void) throw() {
    atomic_store_strong(&m_is_dead, true);
}

bool manager_state::capture_is_enabled(void) throw() {
    return atomic_load_strong(&m_capture_enabled);
}

void manager_state::enable_capture(void) throw() {
    atomic_store_strong(&m_capture_enabled, true);
}

void manager_state::disable_capture(void) throw() {
    atomic_store_strong(&m_capture_enabled, false);
}

bool manager_state::copy_is_enabled(void) throw() {
    return atomic_load_strong(&m_copy_enabled);
}

void manager_state::enable_copy(void) throw() {
    atomic_store_strong(&m_copy_enabled, true);
}

void manager_state::disable_copy(void) throw() {
    atomic_store_strong(&m_copy_enabled, false);
}

