/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include "file_descriptor_map.h"
#include "backup_debug.h"

#include <cstdlib>
#include <pthread.h>
#include <stdio.h>
#include <vector>

class file_description;
template class std::vector<file_description *>;

// This mutx protects the file descriptor map
static pthread_mutex_t get_put_mutex = PTHREAD_MUTEX_INITIALIZER;

////////////////////////////////////////////////////////////////////////////////
//
// file_descriptor_map():
//
// Description: 
//
//     Constructor.
//
file_descriptor_map::file_descriptor_map()
{}

////////////////////////////////////////////////////////////////////////////////
//
// ~file_descriptor_map():
//
// Description: 
//
//     Destructor.
//
file_descriptor_map::~file_descriptor_map()
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
//
// get():
//
// Description: 
//
//     Returns pointer to the file description object that matches the
// given file descriptor.  This will return NULL if the given file
// descriptor has not been added to this map.
//
file_description* file_descriptor_map::get(int fd)
{
    if (HotBackup::MAP_DBG) { 
        printf("get() called with fd = %d \n", fd);
    }
    pthread_mutex_lock(&get_put_mutex);
    file_description *result = this->get_unlocked(fd);
    pthread_mutex_unlock(&get_put_mutex);
    return result;
}

file_description* file_descriptor_map::get_unlocked(int fd) {
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
//
// put():
//
// Description: 
//
//     Allocates a new file description object and inserts it into the map.
// If the given file descriptor is larger than the current size of the array,
// then the array is expanded from it's current length, putting a NULL pointer 
// in each expanded slot.
//
file_description* file_descriptor_map::put(int fd, volatile bool *is_dead)
{
    if (HotBackup::MAP_DBG) { 
        printf("put() called with fd = %d \n", fd);
    }
    
    file_description *description = new file_description(is_dead);
    
    // <CER> Is this to make space for the backup fd?
    // <CER> Shouldn't we do this when we are adding a file descriptor?
    //description->fds.push_back(0); // fd?
    pthread_mutex_lock(&get_put_mutex);
    this->grow_array(fd);
    m_map[fd] = description;
    pthread_mutex_unlock(&get_put_mutex);
    return description;
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

void file_descriptor_map::erase(int fd)
{
    if (HotBackup::MAP_DBG) { 
        printf("erase() called with fd = %d \n", fd);
    }

    pthread_mutex_lock(&get_put_mutex);
    if ((size_t)fd  >= m_map.size()) {
        pthread_mutex_unlock(&get_put_mutex);
    } else {
        file_description *description = m_map[fd];
        m_map[fd] = NULL;
        pthread_mutex_unlock(&get_put_mutex);

        // Do this after releasing the lock
        if (description!=NULL) {
            description->close();
            delete description;
        }
    }
}


////////////////////////////////////////////////////////////////////////////////
//
// size():
//
int file_descriptor_map::size(void)
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
void file_descriptor_map::grow_array(int fd)
{
    if (fd>=0) {
        while(m_map.size() <= (size_t)fd) {
            m_map.push_back(NULL);
        }
    } else {
        // Don't bother complaining if someone manages to pass a negative fd.
    }
}

void lock_file_descriptor_map(void) {
    pthread_mutex_lock(&get_put_mutex);
}
void unlock_file_descriptor_map(void) {
    pthread_mutex_unlock(&get_put_mutex);
}
