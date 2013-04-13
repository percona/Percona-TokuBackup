/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id: file_descriptor_map.h 54931 2013-03-29 19:51:23Z bkuszmaul $"

#ifndef SOURCE_FILE_H
#define SOURCE_FILE_H

#include "description.h"
#include <stdint.h>

struct range {
    uint64_t lo, hi;
};

class source_file {
public:
    source_file(const char * const path); // the source file owns the path
    ~source_file(void); /// the source file will delete the path.
    int init(void);
    const char * name(void);
    source_file *next(void);
    void set_next(source_file *next);

    int lock_range(uint64_t lo, uint64_t  hi) __attribute__((warn_unused_result));
    // Effect: Lock the range specified by [lo,hi) (that is lo inclusive to hi exclusive).   Blocks until no locked range intersects [lo,hi).  Use hi==LLONG_MAX to specify the whole file.  Return 0 or an error number (if error then report it to backup manager)

    int unlock_range(uint64_t lo, uint64_t hi) __attribute__((warn_unused_result));
    // Effect: Unlock the specified range.  Requires that the range is locked (if we notice a problem we'll return EINVAL).  Return 0 or an error number.

    bool lock_range_would_block_unlocked(uint64_t lo, uint64_t hi);
    // Effect: Return true if lock_range() would block because [lo,hi) intersects some locked range.
    //  This function does not acquire any locks.  We expose it for testing purposes (it should not be used by production code).x


    // Name locking and associated rename call.
    int name_write_lock(void);
    int name_read_lock(void);
    int name_unlock(void);
    int rename(const char * new_name);

    // Note: These three methods are not inherintly thread safe.
    // They must be protected with a mutex.
    void add_reference(void);
    void remove_reference(void);
    unsigned int get_reference_count(void);
private:
    char * m_full_path; // the source_file owns this.
    source_file *m_next;
    pthread_rwlock_t m_name_rwlock;
    unsigned int m_reference_count;

    pthread_mutex_t m_mutex;
    pthread_cond_t  m_cond;
    std::vector<struct range> m_locked_ranges;
};

#endif // End of header guardian.
