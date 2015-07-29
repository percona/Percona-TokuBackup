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
#ifndef RWLOCK_H
#define RWLOCK_H

#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include <pthread.h>

extern void prwlock_rdlock(pthread_rwlock_t *) throw();
extern void prwlock_wrlock(pthread_rwlock_t *) throw();
extern void prwlock_unlock(pthread_rwlock_t *) throw();
extern void prwlock_rdlock(pthread_rwlock_t *, const backtrace) throw();
extern void prwlock_wrlock(pthread_rwlock_t *, const backtrace) throw();
extern void prwlock_unlock(pthread_rwlock_t *, const backtrace) throw();

class with_rwlock_rdlocked {
  private:
    pthread_rwlock_t *m_rwlock; 
    bool m_have_backtrace;
    const backtrace m_backtrace;
  public:
    with_rwlock_rdlocked(pthread_rwlock_t *rw): m_rwlock(rw), m_have_backtrace(false) {
        prwlock_rdlock(m_rwlock);
    }
    with_rwlock_rdlocked(pthread_rwlock_t *m, const backtrace bt): m_rwlock(m), m_have_backtrace(true), m_backtrace(bt) {
        prwlock_rdlock(m_rwlock, m_backtrace);
    }
    ~with_rwlock_rdlocked(void) {
        if (m_have_backtrace) {
            prwlock_unlock(m_rwlock, m_backtrace);
        } else {
            prwlock_unlock(m_rwlock);
        }
    }
};

class with_rwlock_wrlocked {
  private:
    pthread_rwlock_t *m_rwlock; 
    bool m_have_backtrace;
    const backtrace m_backtrace;
  public:
    with_rwlock_wrlocked(pthread_rwlock_t *rw): m_rwlock(rw), m_have_backtrace(false) {
        prwlock_wrlock(m_rwlock);
    }
    with_rwlock_wrlocked(pthread_rwlock_t *m, const backtrace bt): m_rwlock(m), m_have_backtrace(true), m_backtrace(bt) {
        prwlock_wrlock(m_rwlock, m_backtrace);
    }
    ~with_rwlock_wrlocked(void) {
        if (m_have_backtrace) {
            prwlock_unlock(m_rwlock, m_backtrace);
        } else {
            prwlock_unlock(m_rwlock);
        }
    }
};


#endif // End of header guardian.
