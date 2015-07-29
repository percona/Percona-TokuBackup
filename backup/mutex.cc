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
#include <stdio.h>
#include "manager.h"

#define NO_DEPRECATE_PTHREAD_MUTEXES
#include "mutex.h"
#include "check.h"

void pmutex_lock(pthread_mutex_t *mutex, const backtrace bt) throw() {
    int r = pthread_mutex_lock(mutex);
    if (r != 0) {
        printf("HotBackup::pmutex_lock() failed, r = %d", r);
    }
    check_bt(r==0, bt);
}

void pmutex_unlock(pthread_mutex_t *mutex, const backtrace bt) throw() {
    int r = pthread_mutex_unlock(mutex);
    if (r != 0) {
        printf("HotBackup::pmutex_unlock() failed, r = %d", r);
    }
    check_bt(r==0, bt);
}

void pmutex_lock(pthread_mutex_t *mutex) throw() {
    pmutex_lock(mutex, BACKTRACE(NULL));
}

void pmutex_unlock(pthread_mutex_t *mutex) throw() {
    pmutex_unlock(mutex, BACKTRACE(NULL));
}
