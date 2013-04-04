/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:  
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include "backup_manager.h"
#include "real_syscalls.h"
#include "backup_debug.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <valgrind/helgrind.h>

#if DEBUG_HOTBACKUP
#define WARN(string, arg) HotBackup::CaptureWarn(string, arg)
#define TRACE(string, arg) HotBackup::CaptureTrace(string, arg)
#define ERROR(string, arg) HotBackup::CaptureError(string, arg)
#else
#define WARN(string, arg) 
#define TRACE(string, arg) 
#define ERROR(string, arg) 
#endif


///////////////////////////////////////////////////////////////////////////////
//
// backup_manager() -
//
// Description: 
//
//     Constructor.
//
backup_manager::backup_manager(void)
    : m_start_copying(true),
      m_keep_capturing(false),
      m_is_capturing(false),
      m_backup_is_running(false),
      m_session(NULL),
      m_throttle(ULONG_MAX),
      m_an_error_happened(false),
      m_errnum(BACKUP_SUCCESS),
      m_errstring(NULL)
{
    int r = pthread_mutex_init(&m_mutex, NULL);
    VALGRIND_HG_DISABLE_CHECKING(&m_backup_is_running, sizeof(m_backup_is_running));
    VALGRIND_HG_DISABLE_CHECKING(&m_is_dead, sizeof(m_is_dead));
    m_is_dead = false;
    if (r!=0) {
        int e = errno;
        fprintf(stderr, "Backup manager failed to initialize mutex: %s:%d errno=%d (%s)\n", __FILE__, __LINE__, e, strerror(e));
        m_is_dead = true;
        return;
    }
    r = pthread_mutex_init(&m_session_mutex, NULL);
    if (r!=0) {
        int e = errno;
        fprintf(stderr, "Backup manager failed to initialize mutex: %s:%d errno=%d (%s)\n", __FILE__, __LINE__, e, strerror(e));
        m_is_dead = true;
        return;
    }
    r = pthread_mutex_init(&m_error_mutex, NULL);
    if (r!=0) {
        int e = errno;
        fprintf(stderr, "Backup manager failed to initialize mutex: %s:%d errno=%d (%s)\n", __FILE__, __LINE__, e, strerror(e));
        m_is_dead = true;
        return;
    }
}

backup_manager::~backup_manager(void) {
    pthread_mutex_destroy(&m_mutex);
    pthread_mutex_destroy(&m_session_mutex);
    pthread_mutex_destroy(&m_error_mutex);
    if (m_errstring) free(m_errstring);
}

///////////////////////////////////////////////////////////////////////////////
//
// do_backup() -
//
// Description: 
//
//     
//
int backup_manager::do_backup(const char *source, const char *dest, backup_callbacks *calls) {
    int r;
    if (m_is_dead) {
        calls->report_error(-1, "Backup system is dead");
        r = -1;
        goto error_out;
    }
    m_backup_is_running = true;
    r = calls->poll(0, "Preparing backup");
    if (r != 0) {
        calls->report_error(r, "User aborted backup");
        goto error_out;
    }

    r = pthread_mutex_trylock(&m_mutex);
    if (r != 0) {
        if (r==EBUSY) {
            calls->report_error(r, "Another backup is in progress.");
            goto error_out;
        }
        goto error_out;
    }
    
    pthread_mutex_lock(&m_session_mutex);
    m_session = new backup_session(source, dest, calls, &r);
    pthread_mutex_unlock(&m_session_mutex);
    if (r!=0) {
        goto unlock_out;
    }
    
    r = this->prepare_directories_for_backup(m_session);
    if (r != 0) {
        goto disable_out;
    }

    VALGRIND_HG_DISABLE_CHECKING(&m_is_capturing, sizeof(m_is_capturing));
    m_is_capturing = true;

    while (!m_start_copying) sched_yield();

    r = m_session->do_copy();
    if (r != 0) {
        // This means we couldn't start the copy thread (ex: pthread error).
        goto disable_out;
    }

disable_out:
    // If the client asked us to keep capturing till they tell us to stop, then do what they said.
    while (m_keep_capturing) sched_yield();

    m_backup_is_running = false;
    this->disable_descriptions();

    VALGRIND_HG_DISABLE_CHECKING(&m_is_capturing, sizeof(m_is_capturing));
    m_is_capturing = false;

unlock_out:

    pthread_mutex_lock(&m_session_mutex);
    delete m_session;
    m_session = NULL;
    pthread_mutex_unlock(&m_session_mutex);

    {
        int pthread_error = pthread_mutex_unlock(&m_mutex);
        if (pthread_error != 0) {
            // TODO: Should there be a way to disable backup permanently in this case?
            calls->report_error(pthread_error, "pthread_mutex_unlock failed.  Backup system is probably broken");
            if (r != 0) {
                r = pthread_error;
            }
        }
    }

    if (m_an_error_happened) {
        calls->report_error(m_errnum, m_errstring);
        r = m_errnum;
    }

error_out:
    return r;
}

///////////////////////////////////////////////////////////////////////////////
//
int backup_manager::prepare_directories_for_backup(backup_session *session)
{
    int r = 0;
    // Loop through all the current file descriptions and prepare them
    // for backup.
    lock_file_descriptor_map(); // TODO: This lock is much too coarse.  Need to refine it.  This lock deals with a race between file->create() and a close() call from the application.  We aren't using the m_refcount in file_description (which we should be) and we even if we did, the following loop would be racy since m_map.size could change while we are running, and file descriptors could come and go in the meanwhile.  So this really must be fixed properly to refine this lock.
    for (int i = 0; i < m_map.size(); ++i) {
        file_description *file = m_map.get_unlocked(i);
        if (file == NULL) {
            continue;
        }
        
        const char * source_path = file->get_full_source_name();
        if (!session->is_prefix(source_path)) {
            continue;
        }

        char * file_name = session->translate_prefix(source_path);
        file->prepare_for_backup(file_name);
        int r = open_path(file_name);
        free(file_name);
        if (r != 0) {
            // TODO: Could not open path, abort backup.
            session->abort();
            goto out;
        }

        r = file->create();
        if (r != 0) {
            // TODO: Could not create the file, abort backup.
            session->abort();
            goto out;
        }
    }
    
out:
    unlock_file_descriptor_map();
    return r;
}

///////////////////////////////////////////////////////////////////////////////
//
void backup_manager::disable_descriptions(void)
{
    printf("disabling\n");
    lock_file_descriptor_map();
    for (int i = 0; i < m_map.size(); ++i) {
        file_description *file = m_map.get_unlocked(i);
        if (file == NULL) {
            continue;
        }
        
        file->disable_from_backup();
        printf("sleeping\n"); usleep(1000);
    }
    unlock_file_descriptor_map();
}

///////////////////////////////////////////////////////////////////////////////
//
// create() -
//
// Description:
//
//     TBD: How is create different from open?  Is the only
// difference that we KNOW the file doesn't yet exist (from
// copy) for the create case?
//
void backup_manager::create(int fd, const char *file) 
{
    if (m_is_dead) return; 
    TRACE("entering create() with fd = ", fd);
    m_map.put(fd, &m_is_dead);
    file_description *description = m_map.get(fd);
    description->set_full_source_name(file);
    
    pthread_mutex_lock(&m_session_mutex);
    
    if (m_session != NULL) {
        char *backup_file_name = m_session->capture_create(file);
        if (backup_file_name != NULL) {
            description->prepare_for_backup(backup_file_name);
            int r = description->create();
            if(r != 0) {
                // TODO: abort backup, creation of backup file failed.
                m_session->abort();
            }

            free((void*)backup_file_name);
        }
    }
    
    pthread_mutex_unlock(&m_session_mutex);
}


///////////////////////////////////////////////////////////////////////////////
//
// open() -
//
// Description: 
//
//     If the given file is in our source directory, this method
// creates a new file_description object and opens the file in
// the backup directory.  We need the bakcup copy open because
// it may be updated if and when the user updates the original/source
// copy of the file.
//
void backup_manager::open(int fd, const char *file, int oflag)
{
    if (m_is_dead) return;
    TRACE("entering open() with fd = ", fd);
    m_map.put(fd, &m_is_dead);
    file_description *description = m_map.get(fd);
    description->set_full_source_name(file);
    
    pthread_mutex_lock(&m_session_mutex);

    if(m_session != NULL) {
        char *backup_file_name = m_session->capture_open(file);
        if(backup_file_name != NULL) {
            description->prepare_for_backup(backup_file_name);
            int r = description->open();
            if(r != 0) {
                // TODO: abort backup, open failed.
                m_session->abort();
            }
            
            free((void*)backup_file_name);
        }
    }

    pthread_mutex_unlock(&m_session_mutex);

    // TODO: Remove this dead code.
    oflag++;
}


///////////////////////////////////////////////////////////////////////////////
//
// close() -
//
// Description:
//
//     Find and deallocate file description based on incoming fd.
//
void backup_manager::close(int fd) 
{
    if (m_is_dead) return;
    TRACE("entering close() with fd = ", fd);
    m_map.erase(fd); // If the fd exists in the map, close it and remove it from the mmap.
}


///////////////////////////////////////////////////////////////////////////////
//
// write() -
//
// Description: 
//
//     Using the given file descriptor, this method updates the 
// backup copy of a prevously opened file.
//     Also does the write itself (the write is in here so that a lock can be obtained to protect the file offset)
//
ssize_t backup_manager::write(int fd, const void *buf, size_t nbyte)
{
    if (m_is_dead) return call_real_write(fd, buf, nbyte);
    TRACE("entering write() with fd = ", fd);
    file_description *description = m_map.get(fd);
    ssize_t r = 0;
    if (description == NULL) {
        r = call_real_write(fd, buf, nbyte);
    } else {
        description->lock();
        r = call_real_write(fd, buf, nbyte);
        // TODO: Don't call our write if the first one fails.
        description->write(r, buf);
        description->unlock();
    }
    
    return r;
}


///////////////////////////////////////////////////////////////////////////////
//
// read() -
//
// Description:
//
//     Do the read.
//
ssize_t backup_manager::read(int fd, void *buf, size_t nbyte) {
    if (m_is_dead) return call_real_read(fd, buf, nbyte);
    ssize_t r = 0;
    TRACE("entering write() with fd = ", fd);
    file_description *description = m_map.get(fd);
    if (description == NULL) {
        r = call_real_read(fd, buf, nbyte);
    } else {
        description->lock();
        r = call_real_read(fd, buf, nbyte);
        // TODO: Don't perform our read if the first one fails.
        description->read(r);
        description->unlock();
    }
    
    return r;
}

///////////////////////////////////////////////////////////////////////////////
//
// pwrite() -
//
// Description:
//
//     Same as regular write, but uses additional offset argument
// to write to a particular position in the backup file.
//
void backup_manager::pwrite(int fd, const void *buf, size_t nbyte, off_t offset)
// Do the write.  If the destination gets a short write, that's an error.
{
    if (m_is_dead || !m_backup_is_running) return;
    TRACE("entering pwrite() with fd = ", fd);

    file_description *description = NULL;
    description = m_map.get(fd);
    if (description == NULL) {
        return;
    }

    int r = description->pwrite(buf, nbyte, offset);
    if(r != 0) {
        set_error(r, "failed write while backing up %s", description->get_full_source_name());
    }
}


///////////////////////////////////////////////////////////////////////////////
//
// seek() -
//
// Description: 
//
//     Move the backup file descriptor to the new position.  This allows
// upcoming intercepted writes to be backed up properly.
//
off_t backup_manager::lseek(int fd, size_t nbyte, int whence) {
    if (m_is_dead) return call_real_lseek(fd, nbyte, whence);
    TRACE("entering seek() with fd = ", fd);
    file_description *description = NULL;
    description = m_map.get(fd);
    if (description == NULL) {
        return call_real_lseek(fd, nbyte, whence);
    } else {
        description->lock();
        off_t new_offset = call_real_lseek(fd, nbyte, whence);
        description->lseek(new_offset);
        description->unlock();
        return new_offset;
    }
}


///////////////////////////////////////////////////////////////////////////////
//
// rename() -
//
// Description:
//
//     TBD...
//
void backup_manager::rename(const char *oldpath, const char *newpath)
{
    if (m_is_dead) return;
    TRACE("entering rename()...", "");
    TRACE("-> old path = ", oldpath);
    TRACE("-> new path = ", newpath);
    // TODO:
    oldpath++;
    newpath++;
}

///////////////////////////////////////////////////////////////////////////////
//
// ftruncate() -
//
// Description:
//
//     TBD...
//
void backup_manager::ftruncate(int fd, off_t length)
{
    if (m_is_dead) return;
    int r = 0;
    TRACE("entering ftruncate with fd = ", fd);
    file_description *description = m_map.get(fd);
    if (description != NULL) {
        r = description->truncate(length);
    }
    
    if(r != 0) {
        // TODO: Abort the backup, truncate failed on the file.
    }
}

///////////////////////////////////////////////////////////////////////////////
//
// truncate() -
//
// Description:
//
//     TBD...
//
void backup_manager::truncate(const char *path, off_t length)
{
    if (m_is_dead) return;
    TRACE("entering truncate() with path = ", path);
    // TODO:
    // 1. Convert the path to the backup dir.
    // 2. Call real_ftruncate directly.
    if(path) {
        length++;
    }
}

///////////////////////////////////////////////////////////////////////////////
//
// mkdir() -
//
// Description:
//
//     TBD...
//
void backup_manager::mkdir(const char *pathname)
{
    if (m_is_dead) return;
    pthread_mutex_lock(&m_session_mutex);
    if(m_session != NULL) {
        m_session->capture_mkdir(pathname);
    }

    pthread_mutex_unlock(&m_session_mutex);
}


///////////////////////////////////////////////////////////////////////////////
//
void backup_manager::set_throttle(unsigned long bytes_per_second) {
    VALGRIND_HG_DISABLE_CHECKING(&m_throttle, sizeof(m_throttle));
    m_throttle = bytes_per_second;
    //__atomic_store(&m_throttle, &bytes_per_second, __ATOMIC_SEQ_CST); // sequential consistency is probably too much, but this isn't called often
}

///////////////////////////////////////////////////////////////////////////////
//
unsigned long backup_manager::get_throttle(void) {
    return m_throttle;
    //unsigned long ret;
    //__atomic_load(&m_throttle, &ret, __ATOMIC_SEQ_CST);
    //return ret;
}

void backup_manager::set_error(int errnum, const char *format_string, ...) {
    va_list ap;
    va_start(ap, format_string);

    m_backup_is_running = false;
    {
        int r = pthread_mutex_lock(&m_error_mutex);
        if (r!=0) {
            m_is_dead = true;  // go ahead and set the error, however
        }
    }
    if (!m_an_error_happened) {
        int len = 2*PATH_MAX + strlen(format_string) + 1000; // should be big enough for all our errors
        char *string = (char*)malloc(len);
        int nwrote = vsnprintf(string, len, format_string, ap);
        snprintf(string+nwrote, len-nwrote, "   error %d (%s)", errnum, strerror(errnum));
        if (m_errstring) free(m_errstring);
        m_errstring = string;
        m_errnum = errnum;
        m_an_error_happened = true;
    }
    {
        int r = pthread_mutex_unlock(&m_error_mutex);
        if (r!=0) {
            m_is_dead = true;
        }
    }
    va_end(ap);
    
}

// Test routines.
void backup_manager::set_keep_capturing(bool keep_capturing) {
    VALGRIND_HG_DISABLE_CHECKING(&m_keep_capturing, sizeof(m_keep_capturing));
    m_keep_capturing = keep_capturing;
}

bool backup_manager::is_capturing(void) {
    return m_is_capturing;
}

void backup_manager::set_start_copying(bool start_copying) {
    VALGRIND_HG_DISABLE_CHECKING(&m_start_copying, sizeof(m_start_copying));
    m_start_copying = start_copying;
}
