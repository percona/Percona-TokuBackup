/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id: file_descriptor_map.h 54931 2013-03-29 19:51:23Z bkuszmaul $"

#ifndef SOURCE_FILE_H
#define SOURCE_FILE_H

#include "file_description.h"
#include <stdint.h>

class source_file {
public:
    source_file(const char * const path); // the source file owns the path
    ~source_file(void); /// the source file will delete the path.
    int init(void);
    const char * name(void);
    source_file *next(void);
    void set_next(source_file *next);
    void lock_range(uint64_t lo, uint64_t  hi);  // Effect: Lock the range specified by lo (inclusive) .. hi (exclusive).   Use hi==LLONG_MAX to specify the whole file.
    void unlock_range(uint64_t lo, uint64_t hi);
    // Note: These three methods are not inherintly thread safe.
    // They must be protected with a mutex.
    void add_reference(void);
    void remove_reference(void);
    unsigned int get_reference_count(void);
private:
    char * const m_full_path; // the source_file owns this.
    source_file *m_next;
    pthread_mutex_t m_mutex;
    unsigned int m_reference_count;
};

#endif // End of header guardian.
