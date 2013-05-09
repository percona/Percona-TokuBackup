/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#ifndef DESCRIPTION_H
#define DESCRIPTION_H

#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include <pthread.h>
#include <sys/types.h>
#include <vector>

class source_file;

class description {
private:
    off_t m_offset;            // The offset that is moved by read(), write() and lseek().
    source_file *m_source_file;
    pthread_mutex_t m_mutex;   // A mutex used to make m_offset move atomically when we perform a write (or read).

public:
    description();
    ~description(void);
    int init(void) __attribute__((warn_unused_result));
    // Effect: Initialize a description.  (Note that the constructor isn't allowed to do anything meaningful, since error handling is tricky.
    //  Return 0 on success, otherwise inform the backup manager of the error (fatal_error or backup_error) and return the error code.
    void set_source_file(source_file *file);
    source_file * get_source_file(void) const;
    void lock(void);
    void unlock(void);

    void lseek(off_t new_offset);        
    void increment_offset(ssize_t nbyte);
    off_t get_offset(void); // return the current offset.
};

#endif // end of header guardian.
