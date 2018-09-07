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

#include "backup_debug.h"
#include "fmap.h"
#include "glassbox.h"
#include "manager.h"
#include "mutex.h"
#include "source_file.h"

#include <cstdlib>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <vector>

// This mutx protects the file descriptor map
static pthread_mutex_t get_put_mutex = PTHREAD_MUTEX_INITIALIZER;

////////////////////////////////////////////////////////////////////////////////
//
// fmap():
//
// Description: 
//
//     Constructor.
//
fmap::fmap() throw() {}

////////////////////////////////////////////////////////////////////////////////
//
// ~fmap():
//
// Description: 
//
//     Destructor.
//
fmap::~fmap() throw() {
    for(std::vector<description *>::size_type i = 0; i < m_map.size(); ++i)
    {
        description *file = m_map[i];
        if (file == NULL) {
            continue;
        }
        
        delete file;
        m_map[i] = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////
// Description:  See fmap.h.
void fmap::get(int fd, description** resultp, const backtrace bt) throw() {
    if (HotBackup::MAP_DBG) { 
        printf("get() called with fd = %d \n", fd);
    }
    with_fmap_locked ml(BACKTRACE(&bt));
    description *result = this->get_unlocked(fd);
    *resultp = result;
}

description* fmap::get_unlocked(int fd) throw() {
    if (fd < 0) return NULL;
    description *result;
    if ((size_t)fd >= m_map.size()) {
        result = NULL;
    } else {
        result = m_map[fd];
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////
void fmap::put(int fd, description *file) throw() {
    with_fmap_locked ml(BACKTRACE(NULL));
    this->grow_array(fd);
    glass_assert(m_map[fd] == NULL);
    m_map[fd] = file;
}

////////////////////////////////////////////////////////////////////////////////
//
// erase():
//
// Description: 
//
//     Frees the file description object at the given index.  It also sets
// that index's file descriptor pointer to zero.
//
// Requires: the fd is something currently mapped.

int fmap::erase(int fd, const backtrace bt) throw() {
    with_fmap_locked ml(BACKTRACE(&bt));
    if ((size_t)fd  >= m_map.size()) {
        return 0;
    } else {
        description *description = m_map[fd];
        m_map[fd] = NULL;
        {
            if (description) {
                // Do this after releasing the lock
                //                source_file * src_file = description->get_source_file();
                //if (src_file != NULL) {
                //  src_file->remove_reference();
                //}
                delete description;
            }
            return 0;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// size():
//
int fmap::size(void) throw() {
    return  m_map.size();
}

////////////////////////////////////////////////////////////////////////////////
//
// grow_fds_array():
//
// Description:
//
//     Expands the array of file descriptions to include the given file
// descriptor (fd) index.  This will fill each new spot between the current
// end of the array and the new index with NULL pointers.  These missing 
// file descriptors may be used by the parent process, but not part of our
// backup directory.
// 
// Requires: the get_put_mutex is held
void fmap::grow_array(int fd) throw() {
    if (fd>=0) {
        while(m_map.size() <= (size_t)fd) {
            m_map.push_back(NULL);
        }
    } else {
        // Don't bother complaining if someone manages to pass a negative fd.
    }
}

void fmap::lock_fmap(const backtrace bt) throw() {
    pmutex_lock(&get_put_mutex, BACKTRACE(&bt));
}

void fmap::unlock_fmap(const backtrace bt) throw() {
    pmutex_unlock(&get_put_mutex, BACKTRACE(&bt));
}

// Instantiate the templates we need
template class std::vector<description *>;
