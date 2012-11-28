/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#include "file_descriptor_map.h"
#include "backup_debug.h"
#include <assert.h>
#include <cstdlib>
#include <stdio.h>

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
    if (MAP_DBG) { 
        printf("get() called with fd = %d \n", fd);
    }

    assert(fd >= 0);
    if (fd >= m_map.size()) {
        return NULL;
    }
    
    return m_map.at(fd);
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
void file_descriptor_map::put(int fd)
{
    if (MAP_DBG) { 
        printf("put() called with fd = %d \n", fd);
    }
    // Allocate file description object.
    // TODO: use 'new', since that will call the constructor and initialize
    // our object via the intitialization list.
    file_description *description = 0;
    description = new file_description;
    //description = (file_description*)malloc(sizeof(file_description));
    //description->refcount = 1;
    //description->offset   = 0;
    
    // <CER> Is this to make space for the backup fd?
    // <CER> Shouldn't we do this when we are adding a file descriptor?
    //description->fds.push_back(0); // fd?
    this->grow_array(fd);
    std::vector<file_description*>::iterator it;
    it = m_map.begin();
    it += fd;
    m_map.insert(it, description);
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
void file_descriptor_map::erase(int fd)
{
    if (MAP_DBG) { 
        printf("erase() called with fd = %d \n", fd);
    }

    file_description *description = this->get(fd);
    assert(description != 0);
    delete description;
    m_map.at(fd) = 0;
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
void file_descriptor_map::grow_array(int fd)
{
    assert(fd >= 0);
    while(m_map.size() <= fd) {
        m_map.push_back(NULL);
    }
}

