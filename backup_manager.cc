/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:  
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include "backup_manager.h"
#include "real_syscalls.h"
#include "backup_debug.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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
backup_manager::backup_manager() 
    : m_doing_backup(false),
      m_doing_copy(true), // <CER> Set to false to turn off copy, for debugging purposes.
      m_session(NULL),
      m_throttle(ULONG_MAX)
{
    int r = pthread_mutex_init(&m_mutex, NULL);
    assert(r == 0);
    r = pthread_mutex_init(&m_session_mutex, NULL);
    assert(r == 0);
}


///////////////////////////////////////////////////////////////////////////////
//
// do_backup() -
//
// Description: 
//
//     
//
int backup_manager::do_backup(const char *source, const char *dest, backup_callbacks calls) {
    
    int r = calls.poll(0, "Preparing backup");
    if (r != 0) {
        calls.report_error(r, "User aborted backup");
        goto error_out;
    }

    r = pthread_mutex_trylock(&m_mutex);
    if (r != 0) {
        if (r==EBUSY) {
            calls.report_error(r, "Another backup is in progress.");
            goto error_out;
        }
        goto error_out;
    }
    
    pthread_mutex_lock(&m_session_mutex);
    m_session = new backup_session(source, dest, calls);
    pthread_mutex_unlock(&m_session_mutex);

    if (!m_session->directories_set()) {
        // TODO: Disambiguate between whether the source or destination string does not exist.
        calls.report_error(ENOENT, "Either of the given backup directories do not exist");
        r = ENOENT;
        goto unlock_out;
    }
    
    r = this->prepare_directories_for_backup(*m_session);
    if (r != 0) {
        goto unlock_out;
    }

    r = m_session->do_copy();    
    if (r != 0) {
        // This means we couldn't start the copy thread (ex: pthread error).
        goto unlock_out;
    }

    // TODO: Print the current time after CAPTURE has been disabled.

unlock_out:

    pthread_mutex_lock(&m_session_mutex);
    delete m_session;
    m_session = NULL;
    pthread_mutex_unlock(&m_session_mutex);

    {
        int pthread_error = pthread_mutex_unlock(&m_mutex);
        if (pthread_error != 0) {
            // TODO: Should there be a way to disable backup permanently in this case?
            calls.report_error(pthread_error, "pthread_mutex_unlock failed.  Backup system is probably broken");
            if (r != 0) {
                r = pthread_error;
            }
        }
    }

error_out:
    return r;
}

int open_path(const char *file_path);
///////////////////////////////////////////////////////////////////////////////
//
int backup_manager::prepare_directories_for_backup(backup_session &session)
{
    int r = 0;
    // Loop through all the current file descriptions and prepare them
    // for backup.
    for (int i = 0; i < m_map.size(); ++i) {
        file_description *file = m_map.get(i);
        if (file == NULL) {
            continue;
        }
        
        const char * source_path = file->get_full_source_name();
        if (!session.is_prefix(source_path)) {
            continue;
        }

        char * file_name = session.translate_prefix(source_path);
        file->prepare_for_backup(file_name);
        free(file_name);
        int r = open_path(file_name);
        if (r != 0) {
            // TODO: Could not open path, abort backup.
            session.abort();
            goto out;
        }

        r = file->create();
        if (r != 0) {
            // TODO: Could not create the file, abort backup.
            session.abort();
            goto out;
        }
    }
    
out:
    return r;
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
    TRACE("entering create() with fd = ", fd);
    m_map.put(fd);
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
    TRACE("entering open() with fd = ", fd);
    m_map.put(fd);
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
    TRACE("entering write() with fd = ", fd);
    ssize_t r = 0;
    file_description *description = m_map.get(fd);
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
{
    TRACE("entering pwrite() with fd = ", fd);

    file_description *description = NULL;
    description = m_map.get(fd);
    if (description == NULL) {
        return;
    }

    int r = description->pwrite(buf, nbyte, offset);
    if(r != 0) {
        // TODO: abort backup, pwrite on the backup file failed.
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
    pthread_mutex_lock(&m_session_mutex);
    if(m_session != NULL) {
        m_session->capture_mkdir(pathname);
    }

    pthread_mutex_unlock(&m_session_mutex);
}


///////////////////////////////////////////////////////////////////////////////
//
void backup_manager::set_throttle(unsigned long bytes_per_second) {
    __atomic_store(&m_throttle, &bytes_per_second, __ATOMIC_SEQ_CST); // sequential consistency is probably too much, but this isn't called often
}

///////////////////////////////////////////////////////////////////////////////
//
unsigned long backup_manager::get_throttle(void) {
    unsigned long ret;
    __atomic_load(&m_throttle, &ret, __ATOMIC_SEQ_CST);
    return ret;
}
