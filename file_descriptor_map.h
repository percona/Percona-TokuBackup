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

    int get(int fd, file_description**result) __attribute__((warn_unused_result));
    // Effect:   Returns pointer (in *result) to the file description object that matches the
    //   given file descriptor.  This will return NULL if the given file
    //   descriptor has not been added to this map.
    // If an error occurs, report it to the backup manager (fatal_error or backup_error) and return the error number.

    int put(int fd, file_description**result) __attribute__((warn_unused_result));
    // Effect:    Allocates a new file description object and inserts it into the map.
    // Implementation note: The array may need to be expanded.  Put a NULL pointer in each unused slot.
    // If an error occurs, report it to the backup manager and return the error code.

    file_description* get_unlocked(int fd); // use this one instead of get() when you already have the lock.
    int erase(int fd) __attribute__((warn_unused_result)); // returns 0 or an error number.
    int size(void);
private:
    void grow_array(int fd);
    
friend class file_descriptor_map_unit_test;
};

// Global locks used when the file descriptor map is updated.   Sometimes the backup system needs to hold the lock for several operations.
void lock_file_descriptor_map(void);
void unlock_file_descriptor_map(void);

#endif // End of header guardian.
