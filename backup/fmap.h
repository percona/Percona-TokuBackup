/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ifndef FMAP_H
#define FMAP_H

#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"


#include <vector>
#include "description.h"
#include "backup_directory.h"
#include "backtrace.h"

class backup_directory;

class fmap
{
private:
    std::vector<description *> m_map;
public:
    fmap() throw();
    ~fmap() throw();

    void get(int fd, description**result, const backtrace bt) throw();
    // Effect:   Returns pointer (in *result) to the file description object that matches the
    //   given file descriptor.  This will return NULL if the given file
    //   descriptor has not been added to this map.
    // No errors can occur.

    void put(int fd, description *file) throw();
    // Effect: adds given description pointer to array (acquires a lock)

    description* get_unlocked(int fd) throw(); // use this one instead of get() when you already have the lock.
    int erase(int fd, const backtrace bt) throw() __attribute__((warn_unused_result)); // returns 0 or an error number.
    int size(void) throw();
private:
    void grow_array(int fd) throw();
    
    // Global locks used when the file descriptor map is updated.   Sometimes the backup system needs to hold the lock for several operations.
    // No errors are countenanced.
    static void lock_fmap(backtrace bt) throw();
    static void unlock_fmap(backtrace bt) throw();
    friend class with_fmap_locked;

    friend class fmap_unit_test;

};


class with_fmap_locked {
  private:
    const backtrace m_bt;
  public:
    with_fmap_locked(backtrace bt): m_bt(bt) {
        fmap::lock_fmap(m_bt);
    }
    ~with_fmap_locked(void) {
        fmap::unlock_fmap(m_bt);
    }
};


#endif // End of header guardian.
