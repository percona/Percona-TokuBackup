/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#ifndef FILE_DESCRIPTION_H
#define FILE_DESCRIPTION_H

#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include <pthread.h>
#include <sys/types.h>
#include <vector>

class file_description {
private:
    off_t m_offset;
    int m_fd_in_dest_space;   // what is the fd in the destination space?
    char *m_backup_name;       // These two strings are const except for the destructor which calls free().
    char *m_full_source_name;

    pthread_mutex_t m_mutex; // A mutex used to make m_offset move atomically when we perform a write (or read).

    // NOTE: in the 'real' application, we may use another way to name
    // the destination file, like its name.
public:
    file_description(); // if something goes bad, set *is_dead = true.
    ~file_description(void);
    int init(void);
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
    int close(void) __attribute__((warn_unused_result)); // Returns zero on success or an error number (not errno)
    int truncate(off_t offset);
    volatile bool m_in_source_dir; // this is communicating information asynchronously.  It ought to be atomic.

    void increment_offset(ssize_t nbyte);
    off_t get_offset(void); // return the current offset.
};

#endif // end of header guardian.
