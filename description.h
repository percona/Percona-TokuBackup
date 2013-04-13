/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#ifndef DESCRIPTION_H
#define DESCRIPTION_H

#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include <pthread.h>
#include <sys/types.h>
#include <vector>

class description {
private:
    off_t m_offset;            // The offset that is moved by read(), write() and lseek().
    int m_fd_in_dest_space;    // What is the fd in the destination space?
    char *m_backup_name;       // These two strings would be const except for the destructor which calls free().  We'll be getting rid of names in file descriptions soon anyway.
    char *m_full_source_name;

    pthread_mutex_t m_mutex; // A mutex used to make m_offset move atomically when we perform a write (or read).

public:
    description();
    ~description(void);
    int init(void) __attribute__((warn_unused_result));
    // Effect: Initialize a description.  (Note that the constructor isn't allowed to do anything meaningful, since error handling is tricky.
    //  Return 0 on success, otherwise inform the backup manager of the error (fatal_error or backup_error) and return the error code.

    void prepare_for_backup(const char *name);
    void disable_from_backup(void);
    void set_full_source_name(const char *name);
    const char * get_full_source_name(void);
    int lock(void) __attribute__((warn_unused_result));
    int unlock(void) __attribute__((warn_unused_result));
    int open(void);
    int create(void);
    int pwrite(const void *buf, size_t nbyte, off_t offset) __attribute__((warn_unused_result)); // returns zero on success or an error number.  The number written must be equal to nbyte or it's an error.
    void lseek(off_t new_offset);        
    int close(void) __attribute__((warn_unused_result)); // Returns zero on success or an error number (not errno).  It will have reported the error.
    int truncate(off_t offset);
    volatile bool m_in_source_dir; // this is communicating information asynchronously.  It ought to be atomic.

    void increment_offset(ssize_t nbyte);
    off_t get_offset(void); // return the current offset.
};

#endif // end of header guardian.
