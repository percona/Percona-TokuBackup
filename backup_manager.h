 /* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#ifndef BACKUP_MANAGER_H
#define BACKUP_MANAGER_H

#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include "backup.h"
#include "backup_directory.h"
#include "file_description.h"
#include "file_hash_table.h"
#include <sys/types.h>
#include <vector>
#include <pthread.h>

class backup_manager
{
private:
    volatile bool m_pause_disable;
    volatile bool m_start_copying; // For test purposes, we can arrange to initialize the backup but not actually start copying.
    volatile bool m_keep_capturing; // For test purposes, we can arrange to keep capturing the backup until the client tells us to stop.
    volatile bool m_is_capturing;   // Backup manager sets to true when capturing is running, sets to false when capturing has stopped.   We look at m_start_copying after setting m_is_capturing=true.
    volatile bool m_done_copying;   // Backup manager sets this true when copying is done.  Happens after m_is_captring

    volatile bool m_is_dead; // true if some error occured so that the backup system shouldn't try any more.
    volatile bool m_backup_is_running; // true if the backup is running.  This can be accessed without any locks.

    file_descriptor_map m_map;
    file_hash_table m_table;
    static pthread_mutex_t m_mutex; // Used to serialize multiple backup operations.

    static pthread_rwlock_t m_capture_rwlock; // Used to serialize access of CAPTURE boolean flag.
    bool m_capture_enabled;

    backup_session *m_session;
    // TODO: use reader/writer lock:
    static pthread_mutex_t m_session_mutex;

    volatile unsigned long m_throttle;

    // Error handling.
    static pthread_mutex_t m_error_mutex;     // When testing errors grab this mutex. 
    volatile bool m_an_error_happened; // True if an error has happened.  This can be read without the mutex.
    int m_errnum;                      // The error number to be passed to the polling function.
    char *m_errstring;                 // The error string to be passed to the polling function.  This string is malloc'd and owned by the backup_manager.

public:
    backup_manager(void);
    ~backup_manager(void);
    // N.B. the google style guide requires all references to be either labeled as a const, or declared to be pointers.
    // I see no reason to use reference variables.  They are redundant with pointers.
    int do_backup(const char *source, const char *dest, backup_callbacks *calls);

    // Methods used during interposition:
    void create(int fd, const char *file);
    void open(int fd, const char *file, int oflag);
    void close(int fd);
    ssize_t write(int fd, const void *buf, size_t nbyte); // Actually performs the write on fd (so that a lock can be obtained).
    ssize_t pwrite(int fd, const void *buf, size_t nbyte, off_t offset); // Actually performs the write on fd (so that a lock can be obtained).
    ssize_t read(int fd, void *buf, size_t nbyte);        // Actually performs the read (so a lock can be obtained).
    off_t   lseek(int fd, size_t nbyte, int whence);      // Actually performs the seek (so a lock can be obtained).
    void rename(const char *oldpath, const char *newpath);
    int ftruncate(int fd, off_t length);                  // Actually performs the trunate (so a lock can be obtained).
    void truncate(const char *path, off_t length);
    void mkdir(const char *pathname);
    
    void set_throttle(unsigned long bytes_per_second); // This is thread-safe.
    unsigned long get_throttle(void);                 // This is thread-safe.

    int turn_on_capture(void); // This is thread safe.
    int turn_off_capture(void); // This is thread safe.

    void set_error(int errnum, const char *format, ...) __attribute__((format(printf,3,4))); 
    // Effect: Set the error information and turn off the backup.  The backup manager isn't dead
    //  so the user could try doing backup again.
    //  This function adds information stating what the errnum is (so don't call strerror from the
    //  caller.

    // TODO: #6537 Factor the test interface out of the main class, cleanly.
    // Test interface.  We'd probably like to compile all this stuff away in production code.
    void pause_disable(bool pause);
    void set_keep_capturing(bool keep_capturing);    // Tell the manager to keep capturing until told not to. This is thread safe.
    bool is_capturing(void);                         // Is the manager capturing?
    bool is_done_copying(void);                      // Is the manager done copying (true sometime after is_capturing)
    void set_start_copying(bool start_copying);     // Tell the manager not to start copying (by passing false) and then to start copying (by passing true). This is thread safe.
    // end of test interface

private:
    int prepare_directories_for_backup(backup_session *session);
    void disable_descriptions(void);
};

#endif // End of header guardian.
