/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
/*======
This file is part of Percona TokuBackup.

Copyright (c) 2006, 2015, Percona and/or its affiliates. All rights reserved.

    Percona TokuBackup is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License, version 2,
    as published by the Free Software Foundation.

     Percona TokuBackup is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Percona TokuBackup.  If not, see <http://www.gnu.org/licenses/>.

----------------------------------------

    Percona TokuBackup is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License, version 3,
    as published by the Free Software Foundation.

    Percona TokuBackup is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with Percona TokuBackup.  If not, see <http://www.gnu.org/licenses/>.
======= */

#ident "$Id$"

#include <pthread.h>

#include "backup_helgrind.h"

#include "manager_state.h"

manager_state::manager_state() throw()
    : m_is_dead(false),
      m_capture_enabled(false),
      m_copy_enabled(false)
{
    TOKUBACKUP_VALGRIND_HG_DISABLE_CHECKING(&m_is_dead,         sizeof(m_is_dead));
    TOKUBACKUP_VALGRIND_HG_DISABLE_CHECKING(&m_capture_enabled, sizeof(m_capture_enabled));
    TOKUBACKUP_VALGRIND_HG_DISABLE_CHECKING(&m_copy_enabled,    sizeof(m_copy_enabled));
}

bool manager_state::is_dead(void) throw() {
    return m_is_dead;
}

bool manager_state::is_alive(void) throw() {
    return !m_is_dead;
}

void manager_state::kill(void) throw() {
    m_is_dead = true;
}

bool manager_state::capture_is_enabled(void) throw() {
    return m_capture_enabled;
}

void manager_state::enable_capture(void) throw() {
    m_capture_enabled = true;
}

void manager_state::disable_capture(void) throw() {
    m_capture_enabled = false;
}

bool manager_state::copy_is_enabled(void) throw() {
    return m_copy_enabled;
}

void manager_state::enable_copy(void) throw() {
    m_copy_enabled = true;
}

void manager_state::disable_copy(void) throw() {
    m_copy_enabled = false;
}

