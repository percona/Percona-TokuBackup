/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#ifndef FILE_DESCRIPTOR_MAP_H
#define FILE_DESCRIPTOR_MAP_H

#include <vector>
#include "file_description.h"
#include "backup_directory.h"

class backup_directory;

class file_descriptor_map
{
private:
    std::vector<file_description *> m_map;
public:
    file_descriptor_map();
    ~file_descriptor_map();
    file_description* get(int fd);
    file_description* put(int fd, volatile bool *is_dead); // create a file description, put it in the map, and return it.  If things go bad, set *is_dead=true.
    file_description* get_unlocked(int fd); // use this one instead of get() when you already have the lock.
    void erase(int fd);
    int size(void);
private:
    void grow_array(int fd);
    
friend class file_descriptor_map_unit_test;
};

// Global locks used when the file descriptor map is updated.   Sometimes the backup system needs to hold the lock for several operations.
void lock_file_descriptor_map(void);
void unlock_file_descriptor_map(void);

#endif // End of header guardian.
