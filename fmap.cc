/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include "fmap.h"
#include "backup_debug.h"
#include "backup_manager.h"

#include <assert.h>
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
        file_description *file = m_map[i];
        if (file == NULL) {
            continue;
        }
        
        delete file;
        m_map[i] = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////
// Description:  See fmap.h.
int fmap::get(int fd, file_description** resultp) {
    if (HotBackup::MAP_DBG) { 
        printf("get() called with fd = %d \n", fd);
    }
    {
        int r = pthread_mutex_lock(&get_put_mutex);
        if (r!=0) {
            manager.fatal_error(r, "Failed to lock mutex %s:%d errno=%d (%s)\n", __FILE__, __LINE__, r, strerror(r));
            return r;
        }
    }
    file_description *result = this->get_unlocked(fd);
    {
        int r = pthread_mutex_unlock(&get_put_mutex);
        if (r!=0) {
            manager.fatal_error(r, "Failed to unlock mutex %s:%d errno=%d (%s)\n", __FILE__, __LINE__, r, strerror(r));
            return r;
        }
    }
    *resultp = result;
    return 0;
}

file_description* fmap::get_unlocked(int fd) {
    if (fd < 0) return NULL;
    file_description *result;
    if ((size_t)fd >= m_map.size()) {
        result = NULL;
    } else {
        result = m_map[fd];
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////
// Description:  See fmap.h.
int fmap::put(int fd, file_description**result) {
    if (HotBackup::MAP_DBG) { 
        printf("put() called with fd = %d \n", fd);
    }
    
    file_description *description = new file_description();
    int r = description->init();
    if (r != 0) {
        *result = NULL;
        return r; // the error has been reported.
    }
    
    // <CER> Is this to make space for the backup fd?
    // <CER> Shouldn't we do this when we are adding a file descriptor?
    //description->fds.push_back(0); // fd?
    pthread_mutex_lock(&get_put_mutex);    // TODO: #6531 handle any errors
    this->grow_array(fd);
    assert(m_map[fd]==NULL);
    m_map[fd] = description;
    pthread_mutex_unlock(&get_put_mutex);    // TODO: #6531 handle any errors
    *result = description;
    return r;
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
    int r = 0;
    if (HotBackup::MAP_DBG) { 
        printf("erase() called with fd = %d \n", fd);
    }

    pthread_mutex_lock(&get_put_mutex);    // TODO: #6531 handle any errors
    if ((size_t)fd  >= m_map.size()) {
        pthread_mutex_unlock(&get_put_mutex);    // TODO: #6531 handle any errors
    } else {
        file_description *description = m_map[fd];
        m_map[fd] = NULL;
        pthread_mutex_unlock(&get_put_mutex);    // TODO: #6531 handle any errors

        // Do this after releasing the lock
        if (description!=NULL) {
            r = description->close();
            delete description;
        }
    }
    return r;
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

void lock_fmap(void) {
    pthread_mutex_lock(&get_put_mutex);    // TODO: #6531 handle any errors
}
void unlock_fmap(void) {
    pthread_mutex_unlock(&get_put_mutex);   // TODO: #6531 handle any errors
}
