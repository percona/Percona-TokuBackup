/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
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

class file_description;
template class std::vector<file_description *>;

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
    for(std::vector<file_description *>::size_type i = 0; i < m_map.size(); ++i)
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
    lock_fmap(BACKTRACE(&bt));
    description *result = this->get_unlocked(fd);
    unlock_fmap(BACKTRACE(&bt));
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
void fmap::put_unlocked(int fd, description *file) throw() {
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
    lock_fmap(BACKTRACE(&bt));
    if ((size_t)fd  >= m_map.size()) {
        unlock_fmap(BACKTRACE(&bt));
        return 0;
    } else {
        description *description = m_map[fd];
        m_map[fd] = NULL;
        {
            unlock_fmap(BACKTRACE(&bt));
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

void lock_fmap(const backtrace bt) throw() {
    pmutex_lock(&get_put_mutex, BACKTRACE(&bt));
}

void unlock_fmap(const backtrace bt) throw() {
    pmutex_unlock(&get_put_mutex, BACKTRACE(&bt));
}

// Instantiate the templates we need
template class std::vector<description*>;
