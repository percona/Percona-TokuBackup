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

#include "backup_callbacks.h"

//////////////////////////////////////////////////////////////////////////////
//
backup_callbacks::backup_callbacks(backup_poll_fun_t poll_fun,
                                   void *poll_extra,
                                   backup_error_fun_t error_fun,
                                   void *error_extra,
                                   backup_exclude_copy_fun_t exclude_copy_fun,
                                   void *exclude_copy_extra,
                                   backup_throttle_fun_t throttle_fun,
                                   backup_before_stop_capt_fun_t bsc_fun,
                                   void *bsc_extra,
                                   backup_after_stop_capt_fun_t asc_fun,
                                   void *asc_extra
) throw()
: m_poll_function(poll_fun),
m_poll_extra(poll_extra),
m_error_function(error_fun),
m_error_extra(error_extra),
m_exclude_copy_function(exclude_copy_fun),
m_exclude_copy_extra(exclude_copy_extra),
m_throttle_function(throttle_fun),
m_bsc_fun(bsc_fun),
m_bsc_extra(bsc_extra),
m_asc_fun(asc_fun),
m_asc_extra(asc_extra)
{}

//////////////////////////////////////////////////////////////////////////////
//
int backup_callbacks::poll(float progress, const char *progress_string) throw() {
    int r = 0;
    r = m_poll_function(progress, progress_string, m_poll_extra);
    return r;
}

//////////////////////////////////////////////////////////////////////////////
//
void backup_callbacks::report_error(int error_number, const char *error_str) throw() {
    m_error_function(error_number, error_str, m_error_extra);
}

//////////////////////////////////////////////////////////////////////////////
//
unsigned long backup_callbacks::get_throttle(void) throw() {
    return m_throttle_function();
}

int backup_callbacks::exclude_copy(const char *source) throw() {
    int r = 0;
    if (m_exclude_copy_function)
        r = m_exclude_copy_function(source, m_exclude_copy_extra);
    return r;
}
