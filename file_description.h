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
    int m_refcount;
    off_t m_offset;
    std::vector<int> m_fds; // which fds refer to this file description.
    int m_fd_in_dest_space;   // what is the fd in the destination space?
    char *m_backup_name;       // These two strings are const except for the destructor which calls free().
    char *m_full_source_name;

    pthread_mutex_t m_mutex; // A mutex used to make m_offset move atomically when we perform a write (or read).

    // NOTE: in the 'real' application, we may use another way to name
    // the destination file, like its name.
public:
    file_description(volatile bool *is_dead); // if something goes bad, set *is_dead = true.
    ~file_description(void);
    void prepare_for_backup(const char *name);
    void disable_from_backup(void);
    void set_full_source_name(const char *name);
    const char * get_full_source_name(void);
    void lock(void);
    void unlock(void);
    int open(void);
    int create(void);
    void write(ssize_t written, const void *buf);
    int pwrite(const void *buf, size_t nbyte, off_t offset); // returns zero on success or an error number.  The number written must be equal to nbyte or it's an error.
    void read(ssize_t nbyte);
    void lseek(off_t new_offset);        
    int close(void);
    int truncate(off_t offset);
    volatile bool m_in_source_dir; // this is communicating information asynchronously.  It ought to be atomic.
};

#endif // end of header guardian.
