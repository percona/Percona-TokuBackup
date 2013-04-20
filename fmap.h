/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#ifndef FMAP_H
#define FMAP_H

#include <vector>
#include "description.h"
#include "backup_directory.h"

class backup_directory;

class fmap
{
private:
    std::vector<description *> m_map;
public:
    fmap();
    ~fmap();

    int get(int fd, description**result) __attribute__((warn_unused_result));
    // Effect:   Returns pointer (in *result) to the file description object that matches the
    //   given file descriptor.  This will return NULL if the given file
    //   descriptor has not been added to this map.
    // If an error occurs, report it to the backup manager (fatal_error or backup_error) and return the error number.
    // If an error occurs *result is not changed.

    int put(int fd, description**result) __attribute__((warn_unused_result));
    // Effect:    Allocates a new file description object and inserts it into the map.
    // Implementation note: The array may need to be expanded.  Put a NULL pointer in each unused slot.
    // If an error occurs, report it to the backup manager and return the error code.
    // If an error occurs *result is not changed.
    void put_unlocked(int fd, description *file);
    // Effect: simply adds given description pointer to array, no locks held.
    description* get_unlocked(int fd); // use this one instead of get() when you already have the lock.
    int erase(int fd) __attribute__((warn_unused_result)); // returns 0 or an error number.
    void erase_unlocked(int fd);
    int size(void);
private:
    void grow_array(int fd);
    
friend class fmap_unit_test;
};

// Global locks used when the file descriptor map is updated.   Sometimes the backup system needs to hold the lock for several operations.
// If an error occurs, it's reported and the error number is returned.  If no error then returns 0.
int lock_fmap(void);
int unlock_fmap(void);

#endif // End of header guardian.
