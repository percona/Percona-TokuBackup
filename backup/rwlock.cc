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
#include "check.h"
#include "manager.h"
#include "mutex.h"
#include "rwlock.h"

void prwlock_rdlock(pthread_rwlock_t *lock, const backtrace bt) throw() {
    int r = pthread_rwlock_rdlock(lock);
    check_bt(r==0, bt);
}

void prwlock_wrlock(pthread_rwlock_t *lock, const backtrace bt) throw() {
    int r = pthread_rwlock_wrlock(lock);
    check_bt(r==0, bt);
}

void prwlock_unlock(pthread_rwlock_t *lock, const backtrace bt) throw() {
    int r = pthread_rwlock_unlock(lock);
    check_bt(r==0, bt);
}

void prwlock_rdlock(pthread_rwlock_t *lock) throw() {
    int r = pthread_rwlock_rdlock(lock);
    check(r==0);
}

void prwlock_wrlock(pthread_rwlock_t *lock) throw() {
    int r = pthread_rwlock_wrlock(lock);
    check(r==0);
}

void prwlock_unlock(pthread_rwlock_t *lock) throw() {
    int r = pthread_rwlock_unlock(lock);
    check(r==0);
}
