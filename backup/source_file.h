/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#ifndef SOURCE_FILE_H
#define SOURCE_FILE_H

#include <stdint.h>

#include "destination_file.h"
#include "description.h"

struct range {
    uint64_t lo, hi;
};

class source_file {
public:
    source_file(const char *path) throw();
    ~source_file(void) throw(); /// the source file will delete the path, if it has been set.
    const char * name(void) const throw();
    source_file *next(void) const throw();
    void set_next(source_file *next) throw();

    void lock_range(uint64_t lo, uint64_t  hi) throw();
    // Effect: Lock the range specified by [lo,hi) (that is lo inclusive to hi exclusive).   Blocks until no locked range intersects [lo,hi).  Use hi==LLONG_MAX to specify the whole file.  No errors can happen (the only possible errors are pthread mutex errors or memory allcoation errors, in which case it's better just to abort).

    int unlock_range(uint64_t lo, uint64_t hi) throw() __attribute__((warn_unused_result));
    // Effect: Unlock the specified range.  Requires that the range is locked (if we notice a problem we'll return EINVAL).  Return 0 or an error number.

    bool lock_range_would_block_unlocked(uint64_t lo, uint64_t hi) const throw();
    // Effect: Return true if lock_range() would block because [lo,hi) intersects some locked range.
    //  This function does not acquire any locks.  We expose it for testing purposes (it should not be used by production code).x

    // Name locking and associated rename call.
    void name_write_lock(void) throw();
    void name_read_lock(void) throw();
    void name_unlock(void) throw();
    int rename(const char * new_name) throw(); // return 0 on success, error number on failure (doesn't set errno)

    // Note: These three methods are not inherintly thread safe.
    // They must be protected with a mutex.
    void add_reference(void) throw();
    void remove_reference(void) throw();
    unsigned int get_reference_count(void) const throw();

    void unlink(void) throw();

    // Methods to manage the lifetime of the destination file
    // corresponding to this source file object.  The lifetime of the
    // destination file should be scoped to a particular backup
    // session object lifetime.
    destination_file * get_destination(void) const throw();
    void set_destination(destination_file * destination) throw();
    void try_to_remove_destination(void) throw();
    int try_to_create_destination_file(const char*) throw();

private:
    char * m_full_path; // the source_file owns this.
    source_file *m_next;
    pthread_rwlock_t m_name_rwlock;
    unsigned int m_reference_count;

    pthread_mutex_t  m_mutex;
    pthread_cond_t   m_cond;
    std::vector<struct range> m_locked_ranges;

    bool m_unlinked;
    destination_file * m_destination_file;
};

#endif // End of header guardian.
