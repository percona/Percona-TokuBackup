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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "backup_debug.h"
#include "check.h"
#include "manager.h"
#include "mutex.h"
#include "description.h"
#include "real_syscalls.h"
#include "source_file.h"

///////////////////////////////////////////////////////////////////////////////
//
description::description() throw()
: m_offset(0),
  m_source_file(NULL)
{
    int r = pthread_mutex_init(&m_mutex, NULL);
    check(r==0);
}

///////////////////////////////////////////////////////////////////////////////
//
description::~description(void) throw()
{
    int r = pthread_mutex_destroy(&m_mutex);
    check(r==0);
}


///////////////////////////////////////////////////////////////////////////////
//
void description::set_source_file(source_file *file) throw() {
    m_source_file = file;
}

///////////////////////////////////////////////////////////////////////////////
//
source_file * description::get_source_file(void) const throw()
{
    return m_source_file;
}

///////////////////////////////////////////////////////////////////////////////
//
void description::lock(const backtrace bt) throw() {
    pmutex_lock(&m_mutex, BACKTRACE(&bt));
}

///////////////////////////////////////////////////////////////////////////////
//
void description::unlock(const backtrace bt) throw() {
    pmutex_unlock(&m_mutex, BACKTRACE(&bt));
}

///////////////////////////////////////////////////////////////////////////////
//
void description::increment_offset(ssize_t nbyte) throw() {
    m_offset += nbyte;
}

///////////////////////////////////////////////////////////////////////////////
//
off_t description::get_offset(void) throw() {    
    return m_offset;
}

///////////////////////////////////////////////////////////////////////////////
//
void description::lseek(off_t new_offset) throw() {
    m_offset = new_offset;
}
