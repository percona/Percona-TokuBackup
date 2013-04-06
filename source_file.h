/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id: file_descriptor_map.h 54931 2013-03-29 19:51:23Z bkuszmaul $"

#include "file_description.h"

#ifndef SOURCE_FILE_H
#define SOURCE_FILE_H

class source_file {
public:
    source_file(const char * const path);
    int init(void);
    const char * const name(void);
    source_file *next(void);
    void set_next(source_file *next);
    void lock_range(void);
    void wait_on_range(void);
    void signal_range(void);
    void unlock_range(void);
    // Note: These three methods are not inherintly thread safe.
    // They must be protected with a mutex.
    void add_reference(void);
    void remove_reference(void);
    unsigned int get_reference_count(void);
private:
    const char * const m_full_path;
    source_file *m_next;
    pthread_mutex_t m_mutex;
    unsigned int m_reference_count;
};

#endif // End of header guardian.
