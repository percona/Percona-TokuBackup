/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include "fmap.h"
#include "backup_debug.h"
#include "manager.h"
#include "glassbox.h"

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
fmap::fmap()
{}

////////////////////////////////////////////////////////////////////////////////
//
// ~fmap():
//
// Description: 
//
//     Destructor.
//
fmap::~fmap()
{
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
int fmap::get(int fd, description** resultp) {
    if (HotBackup::MAP_DBG) { 
        printf("get() called with fd = %d \n", fd);
    }
    {
        int r = lock_fmap();
        if (r!=0) return r;
    }
    description *result = this->get_unlocked(fd);
    {
        int r = unlock_fmap();
        if (r!=0) return r;
    }
    *resultp = result;
    return 0;
}

description* fmap::get_unlocked(int fd) {
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
// Description:  See fmap.h.
int fmap::put(int fd, description**result) {
    if (HotBackup::MAP_DBG) { 
        printf("put() called with fd = %d \n", fd);
    }
    
    description *desc = new description();
    {
        int r = desc->init();
        if (r != 0) {
            *result = NULL;
            return r; // the error has been reported.
        }
    }
    
    {
        int r = lock_fmap();
        if (r!=0) return r;
    }
    this->grow_array(fd);
    glass_assert(m_map[fd]==NULL);
    m_map[fd] = desc;
    {
        int r = unlock_fmap();
        if (r!=0) return r;
    }
    *result = desc;
    return 0;
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

int fmap::erase(int fd) {
    if (HotBackup::MAP_DBG) { 
        printf("erase() called with fd = %d \n", fd);
    }

    {
        int r = lock_fmap();
        if (r!=0) return r;
    }
    if ((size_t)fd  >= m_map.size()) {
        return unlock_fmap();
    } else {
        description *description = m_map[fd];
        m_map[fd] = NULL;
        {
            int r = unlock_fmap();
            int r2 = 0;
            if (description) {
                // Do this after releasing the lock
                r2 = description->close();
                delete description;
            }
            if (r) return r;
            if (r2) return r2;
            return 0;
        }
    }
}


////////////////////////////////////////////////////////////////////////////////
//
// size():
//
int fmap::size(void)
{
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
void fmap::grow_array(int fd)
{
    if (fd>=0) {
        while(m_map.size() <= (size_t)fd) {
            m_map.push_back(NULL);
        }
    } else {
        // Don't bother complaining if someone manages to pass a negative fd.
    }
}

int lock_fmap(void) {
    int r = pthread_mutex_lock(&get_put_mutex);
    if (r!=0) {
        the_manager.fatal_error(r, "Trying to lock mutex at %s:%d", __FILE__, __LINE__);
    }
    return r;
}

int unlock_fmap(void) {
    int r = pthread_mutex_unlock(&get_put_mutex);
    if (r!=0) {
        the_manager.fatal_error(r, "Trying to unlock mutex at %s:%d", __FILE__, __LINE__);
    }
    return r;
}
