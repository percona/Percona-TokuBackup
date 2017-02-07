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

#ifndef BACKUP_CALLBACKS_H
#define BACKUP_CALLBACKS_H

#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include "backup_internal.h"

typedef unsigned long (*backup_throttle_fun_t)(void);

//////////////////////////////////////////////////////////////////////////////
//
class backup_callbacks
{
public:
    backup_callbacks(backup_poll_fun_t poll_fun,
                     void *poll_extra,
                     backup_error_fun_t error_fun,
                     void *error_extra,
                     backup_exclude_copy_fun_t exclude_copy_fun,
                     void *exclude_copy_extra,
                     backup_throttle_fun_t throttle_fun,
                     backup_before_stop_capt_fun_t bsc_fun,
                     void *bsc_extra,
                     backup_after_stop_capt_fun_t asc_fun,
                     void *asc_extra) throw();
    int poll(float progress, const char *progress_string) throw();
    void report_error(int error_number, const char *error_description) throw();
    unsigned long get_throttle(void) throw();
    int exclude_copy(const char *source) throw();
    void before_stop_capt_call() throw() {
        if (m_bsc_fun)
            m_bsc_fun(m_bsc_extra);
    }
    void after_stop_capt_call() throw() {
        if (m_asc_fun)
            m_asc_fun(m_asc_extra);
    }
private:
    backup_callbacks() throw() {};
    backup_poll_fun_t m_poll_function;
    void *m_poll_extra;
    backup_error_fun_t m_error_function;
    void *m_error_extra;
    backup_exclude_copy_fun_t m_exclude_copy_function;
    void *m_exclude_copy_extra;
    backup_throttle_fun_t m_throttle_function;
    backup_before_stop_capt_fun_t m_bsc_fun;
    void *m_bsc_extra;
    backup_after_stop_capt_fun_t m_asc_fun;
    void *m_asc_extra;
};

#endif // end of header guardian.
