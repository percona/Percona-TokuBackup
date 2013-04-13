/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:  
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include "glassbox.h"
#include "manager.h"
#include "real_syscalls.h"
#include "backup_debug.h"
#include "source_file.h"
#include "file_hash_table.h"

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

// TODO: #6530 Get rid of this assert again. 
#include <assert.h>

#if DEBUG_HOTBACKUP
#define WARN(string, arg) HotBackup::CaptureWarn(string, arg)
#define TRACE(string, arg) HotBackup::CaptureTrace(string, arg)
#define ERROR(string, arg) HotBackup::CaptureError(string, arg)
#else
#define WARN(string, arg) 
#define TRACE(string, arg) 
#define ERROR(string, arg) 
#endif

#if PAUSE_POINTS_ON
#define PAUSE(number) while(HotBackup::should_pause(number)) { sleep(2); } printf("Resuming from Pause Point.\n");
#else
#define PAUSE(number)
#endif

pthread_mutex_t manager::m_mutex         = PTHREAD_MUTEX_INITIALIZER;
pthread_rwlock_t manager::m_session_rwlock = PTHREAD_RWLOCK_INITIALIZER;
pthread_mutex_t manager::m_error_mutex   = PTHREAD_MUTEX_INITIALIZER;

///////////////////////////////////////////////////////////////////////////////
//
// manager() -
//
// Description: 
//
//     Constructor.
//
manager::manager(void)
    : 
#ifdef GLASSBOX
      m_pause_disable(false),
      m_start_copying(true),
      m_keep_capturing(false),
      m_is_capturing(false),
      m_done_copying(false),
#endif
      m_backup_is_running(false),
      m_session(NULL),
      m_throttle(ULONG_MAX),
      m_an_error_happened(false),
      m_errnum(BACKUP_SUCCESS),
      m_errstring(NULL)
{
    VALGRIND_HG_DISABLE_CHECKING(&m_backup_is_running, sizeof(m_backup_is_running));
#ifdef GLASSBOX
    VALGRIND_HG_DISABLE_CHECKING(&m_is_capturing, sizeof(m_is_capturing));
    VALGRIND_HG_DISABLE_CHECKING(&m_done_copying, sizeof(m_done_copying));
    VALGRIND_HG_DISABLE_CHECKING(&m_an_error_happened, sizeof(m_an_error_happened));
    VALGRIND_HG_DISABLE_CHECKING(&m_errnum, sizeof(m_errnum));
    VALGRIND_HG_DISABLE_CHECKING(&m_errstring, sizeof(m_errstring));
#endif
}

manager::~manager(void) {
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
int manager::do_backup(const char *source, const char *dest, backup_callbacks *calls) {
    int r = 0;
    if (this->is_dead()) {
        calls->report_error(-1, "Backup system is dead");
        r = -1;
        goto error_out;
    }
    m_an_error_happened = false;
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
    
    // Complain if
    //   1) the backup directory cannot be stat'd (perhaps because it doesn't exist), or
    //   2) The backup directory is not a directory (perhaps because it's a regular file), or
    //   3) the backup directory can not be opened (maybe permissions?), or
    //   4) The backpu directory cannot be readdir'd (who knows what that could be), or
    //   5) The backup directory is not empty (there's a file in there #6542), or
    //   6) The dir cannot be closedir()'d (who knows...)n
    {
        struct stat sbuf;
        r = stat(dest, &sbuf);
        if (r!=0) {
            r = errno;
            char error_msg[4000];
            snprintf(error_msg, sizeof(error_msg), "Problem stat()ing backup directory %s. errno=%d (%s)", dest, r, strerror(r));
            calls->report_error(r, error_msg);
            pthread_mutex_unlock(&m_mutex); // don't worry about errors here.
            goto error_out;
        }
        if (!S_ISDIR(sbuf.st_mode)) {
            char error_msg[4000];
            snprintf(error_msg, sizeof(error_msg), "Backup destination %s is not a directory", dest);
            r = EINVAL;
            calls->report_error(EINVAL, error_msg);
            pthread_mutex_unlock(&m_mutex); // don't worry about errors here.
            goto error_out;
        }
        {
            DIR *dir = opendir(dest);
            if (dir == NULL) {
                r = errno;
                char error_msg[4000];
                snprintf(error_msg, sizeof(error_msg), "Problem opening backup directory %s. errno=%d (%s)", dest, r, strerror(r));
                calls->report_error(r, error_msg);
                goto unlock_out;
            }
            errno = 0;
      again:
            struct dirent const *e = readdir(dir);
            if (e!=NULL &&
                (strcmp(e->d_name, ".")==0 ||
                 strcmp(e->d_name, "..")==0)) {
                goto again;
            } else if (e!=NULL) {
                // That's bad.  The directory should be empty.
                r = EINVAL;
                char error_msg[4000];
                snprintf(error_msg, sizeof(error_msg), "Backup directory %s is not empty", dest);
                calls->report_error(r, error_msg);
                closedir(dir);
                goto unlock_out;
            } else if (errno!=0) {
                r = errno;
                char error_msg[4000];
                snprintf(error_msg, sizeof(error_msg), "Problem readdir()ing backup directory %s. errno=%d (%s)", dest, r, strerror(r));
                calls->report_error(r, error_msg);
                closedir(dir);
                goto unlock_out;
            } else {
                // e==NULL and errno==0 so that's good.  There are no files, except . and ..
                r = closedir(dir);
                if (r!=0) {
                    r = errno;
                    char error_msg[4000];
                    snprintf(error_msg, sizeof(error_msg), "Problem closedir()ing backup directory %s. errno=%d (%s)", dest, r, strerror(r));
                    calls->report_error(r, error_msg);
                    // The dir is already as closed as I can get it.  Don't call closedir again.
                    goto unlock_out;
                }
            }
        }
    }

    pthread_rwlock_wrlock(&m_session_rwlock);  // TODO: #6531 handle any errors 
    m_session = new backup_session(source, dest, calls, &m_table, &r);
    pthread_rwlock_unlock(&m_session_rwlock);   // TODO: #6531 handle any errors
    if (r!=0) {
        goto unlock_out;
    }
    
    r = this->prepare_directories_for_backup(m_session);
    if (r != 0) {
        goto disable_out;
    }

    this->enable_capture();

    WHEN_GLASSBOX( ({
                m_is_capturing = true;
                m_done_copying = false;
                while (!m_start_copying) sched_yield();
            }) );

    r = m_session->do_copy();
    if (r != 0) {
        // This means we couldn't start the copy thread (ex: pthread error).
        goto disable_out;
    }

disable_out:
    WHEN_GLASSBOX( ({
        m_done_copying = true;
        // If the client asked us to keep capturing till they tell us to stop, then do what they said.
        while (m_keep_capturing) sched_yield();
            }) );

    m_backup_is_running = false;

    this->disable_capture();

    this->disable_descriptions();

    WHEN_GLASSBOX(m_is_capturing = false);

unlock_out: // preserves r if r!0

    pthread_rwlock_wrlock(&m_session_rwlock);   // TODO: #6531 handle any errors
    delete m_session;
    m_session = NULL;
    pthread_rwlock_unlock(&m_session_rwlock);   // TODO: #6531 handle any errors

    {
        int pthread_error = pthread_mutex_unlock(&m_mutex);
        if (pthread_error != 0) {
            this->kill(); // disable backup permanently if the pthread mutexes are failing.
            calls->report_error(pthread_error, "pthread_mutex_unlock failed.  Backup system is probably broken");
            if (r == 0) {
                r = pthread_error;
            }
        }
    }

    if (m_an_error_happened) {
        calls->report_error(m_errnum, m_errstring);
        if (r==0) {
            r = m_errnum; // if we already got an error then keep it.
        }
    }

error_out:
    printf("returning %d\n", r);
    return r;
}

///////////////////////////////////////////////////////////////////////////////
//
int manager::prepare_directories_for_backup(backup_session *session) {
    int r = 0;
    // Loop through all the current file descriptions and prepare them
    // for backup.
    lock_fmap(); // TODO: #6532 This lock is much too coarse.  Need to refine it.  This lock deals with a race between file->create() and a close() call from the application.  We aren't using the m_refcount in file_description (which we should be) and we even if we did, the following loop would be racy since m_map.size could change while we are running, and file descriptors could come and go in the meanwhile.  So this really must be fixed properly to refine this lock.
    for (int i = 0; i < m_map.size(); ++i) {
        description *file = m_map.get_unlocked(i);
        if (file == NULL) {
            continue;
        }
        
        const char * source_path = file->get_full_source_name();
        if (!session->is_prefix(source_path)) {
            continue;
        }

        char * file_name = session->translate_prefix(source_path);
        file->prepare_for_backup(file_name);
        r = open_path(file_name);
        free(file_name);
        if (r != 0) {
            session->abort();
            backup_error(r, "Failed to open path");
            goto out;
        }

        r = file->create();
        if (r != 0) {
            session->abort();
            backup_error(r, "Could not create backup file.");
            goto out;
        }
    }
    
out:
    unlock_fmap();
    return r;
}

///////////////////////////////////////////////////////////////////////////////
//
void manager::disable_descriptions(void)
{
    printf("disabling\n");
    lock_fmap();
    const int size = m_map.size();
    const int middle __attribute__((unused)) = size / 2; // used only in glassbox mode.
    for (int i = 0; i < size; ++i) {
        WHEN_GLASSBOX( ({
                    if (middle == i) {
                        TRACE("Pausing on i = ", i); 
                        while (m_pause_disable) { sched_yield(); }
                        TRACE("Done Pausing on i = ", i);
                    }
                }) );
        description *file = m_map.get_unlocked(i);
        if (file == NULL) {
            continue;
        }
        
        file->disable_from_backup();
        //printf("sleeping\n"); usleep(1000);
    }
    unlock_fmap();
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
int manager::create(int fd, const char *file) 
{
    TRACE("entering create() with fd = ", fd);
    description *description;
    int r = m_map.put(fd, &description);
    if (r != 0) {
        // The error has been reported, so just return
        goto out;
    }

    description->set_full_source_name(file);

    // Add description to hash table.
    m_table.lock();
    {
        source_file *source = m_table.get(description->get_full_source_name());
        if (source == NULL) {
            TRACE("Creating new source file in hash map ", fd);
            source = new source_file(description->get_full_source_name());
            source->add_reference();
            r = source->init();
            if (r != 0) {
                // The error has been reported.
                source->remove_reference();
                delete source;
                m_table.unlock();
                goto out;
            }

            m_table.put(source);
        }
    }

    m_table.unlock();
    
    pthread_rwlock_rdlock(&m_session_rwlock); // TODO: #6531 handle any errors
    
    if (m_session != NULL) {
        char *backup_file_name;
        r = m_session->capture_create(file, &backup_file_name);
        if (r==0 && backup_file_name != NULL) {
            description->prepare_for_backup(backup_file_name);
            int r = description->create();
            if(r != 0) {
                m_session->abort();
                backup_error(r, "Could not create backup file");
            }

            free((void*)backup_file_name);
        }
    }
    
    pthread_rwlock_unlock(&m_session_rwlock); // TODO: #6531  handle any errors
out:
    return r;
}


///////////////////////////////////////////////////////////////////////////////
//
// open() -
//
// Description: 
//
//     If the given file is in our source directory, this method
// creates a new description object and opens the file in
// the backup directory.  We need the bakcup copy open because
// it may be updated if and when the user updates the original/source
// copy of the file.
//
int manager::open(int fd, const char *file, int oflag)
{
    TRACE("entering open() with fd = ", fd);
    description *description;
    int r = m_map.put(fd, &description);
    if (r!=0) {
        goto out; // the error has been reported
    }

    description->set_full_source_name(file);

    // Add description to hash table.
    m_table.lock();
    {
        source_file *source = m_table.get(description->get_full_source_name());
        if (source == NULL) {
            TRACE("Creating new source file in hash map ", fd);
            source = new source_file(description->get_full_source_name());
            source->add_reference();
            r = source->init();
            if (r != 0) {
                // The error has been reported.
                source->remove_reference();
                delete source;
                m_table.unlock();
                goto out;
            }

            m_table.put(source);
        }
    }

    m_table.unlock();
    
    pthread_rwlock_rdlock(&m_session_rwlock); // TODO: #6531 handle any errors

    if(m_session != NULL) {
        char *backup_file_name;
        r = m_session->capture_open(file, &backup_file_name);
        if(r==0 && backup_file_name != NULL) {
            description->prepare_for_backup(backup_file_name);
            r = description->open();
            if(r != 0) {
                m_session->abort();
                backup_error(r, "Could not open backup file.");
            }
            
            free((void*)backup_file_name);
        }
    }

    pthread_rwlock_unlock(&m_session_rwlock); // TODO: #6531 handle any errors

    // TODO: #6533 Remove this dead code.
    oflag++;
 out:
    return r;
}


///////////////////////////////////////////////////////////////////////////////
//
// close() -
//
// Description:
//
//     Find and deallocate file description based on incoming fd.
//
void manager::close(int fd) {
    TRACE("entering close() with fd = ", fd);
    int r1 = m_map.erase(fd); // If the fd exists in the map, close it and remove it from the mmap.
    if (r1!=0) {
        return;  // Any errors have been reported.  The caller doesn't care.
    }
    // Any errors have been reported, and the caller doesn't want to hear about them.
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
ssize_t manager::write(int fd, const void *buf, size_t nbyte)
{
    TRACE("entering write() with fd = ", fd);
    description *description;
    int rr = m_map.get(fd, &description);
    if (rr!=0 || description == NULL) {
        return call_real_write(fd, buf, nbyte);
    } else {
        {
            int r = description->lock();
            if (r!=0) {
                return call_real_write(fd, buf, nbyte); // do the write, but don't do anything else.
            }
        }
        m_table.lock();
        source_file * file = m_table.get(description->get_full_source_name());
        m_table.unlock();

        // We need the range lock before calling real lock so that the write into the source and backup are atomic wrt other writes.
        TRACE("Grabbing file range lock() with fd = ", fd);
        const uint64_t lock_start = description->get_offset();
        const uint64_t lock_end   = lock_start + nbyte;

        // We want to release the description->lock ASAP, since it's limiting other writes.
        // We cannot release it before the real write since the real write determines the new m_offset.

        {
            int r = file->lock_range(lock_start, lock_end);
            if (r!=0) {
                // We've reported the error.
                int ignore __attribute__((__unused__)) = description->unlock(); // don't worry about errors
                return call_real_write(fd, buf, nbyte);
            }
        }

        ssize_t r = call_real_write(fd, buf, nbyte);
        if (r>0) {
            // actually wrote something.
            description->increment_offset(r);
            // Now we can release the description lock, since the offset is calculated.
            { int rrr = description->unlock(); assert(rrr==0); } // TODO: #6531 Handle any errors

            // We still have the lock range, which we do with pwrite

            if (this->capture_is_enabled()) {
                TRACE("write() captured with fd = ", fd);
                int rrr = description->pwrite(buf, nbyte, lock_start);
                if (rrr!=0) {
                    backup_error(rrr, "failed write while backing up %s", description->get_full_source_name());
                }
            } 
            TRACE("Releasing file range lock() with fd = ", fd);
            { int rrr = file->unlock_range(lock_start, lock_end);   assert(rrr == 0); }
        }
        return r;
    }
}


///////////////////////////////////////////////////////////////////////////////
//
// read() -
//
// Description:
//
//     Do the read.
//
ssize_t manager::read(int fd, void *buf, size_t nbyte) {
    TRACE("entering write() with fd = ", fd);
    ssize_t r = 0;
    description *description;
    int rr = m_map.get(fd, &description);
    if (rr!=0 || description == NULL) {
        r = call_real_read(fd, buf, nbyte);
    } else {
        {
            int rrr = description->lock();
            if (rrr!=0) {
                // Do the read if there was an error.
                return call_real_read(fd, buf, nbyte);
            }
        }
        r = call_real_read(fd, buf, nbyte);
        printf("%s:%d r=%ld\n", __FILE__, __LINE__, r);
        if (r>0) {
            description->increment_offset(r); //moves the offset
        }
        {
            int rrr = description->unlock();
            assert(rrr==0); // TODO: #6531 Handle any errors
        }
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
ssize_t manager::pwrite(int fd, const void *buf, size_t nbyte, off_t offset)
// Do the write, returning the number of bytes written.
// Note: If the backup destination gets a short write, that's an error.
{
    TRACE("entering pwrite() with fd = ", fd);
    description *description;
    {
        int r = m_map.get(fd, &description);
        if (r!=0 || description == NULL) {
            return call_real_pwrite(fd, buf, nbyte, offset);
        }
    }

    m_table.lock();
    source_file * file = m_table.get(description->get_full_source_name());
    m_table.unlock();
    { int r = file->lock_range(offset, offset+nbyte);   assert(r == 0); }
    ssize_t nbytes_written = call_real_pwrite(fd, buf, nbyte, offset);
    int e = 0;
    if (nbytes_written>0) {
        if (this->capture_is_enabled()) {
            int r = description->pwrite(buf, nbyte, offset);
            if (r!=0) {
                backup_error(r, "failed write while backing up %s", description->get_full_source_name());
            }
        }
    } else if (nbytes_written<0) {
        e = errno; // save the errno
    }
    { int r = file->unlock_range(offset, offset+nbyte); assert(r == 0); }
    if (nbytes_written<0) {
        errno = e; // restore errno
    }
    return nbytes_written;
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
off_t manager::lseek(int fd, size_t nbyte, int whence) {
    TRACE("entering seek() with fd = ", fd);
    description *description;
    int r = m_map.get(fd, &description);
    if (r!=0 || description == NULL) {
        return call_real_lseek(fd, nbyte, whence);
    } else {
        { int rrr = description->lock(); assert(rrr==0); } // TODO: #6531 Handle any errors 
        off_t new_offset = call_real_lseek(fd, nbyte, whence);
        description->lseek(new_offset);
        { int rrr = description->unlock(); assert(rrr==0); } // TODO: #6531 Handle any errors
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
void manager::rename(const char *oldpath, const char *newpath)
{
    TRACE("entering rename()...", "");
    TRACE("-> old path = ", oldpath);
    TRACE("-> new path = ", newpath);
    // TODO: #6529
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
int manager::ftruncate(int fd, off_t length)
{
    TRACE("entering ftruncate with fd = ", fd);
    // TODO: Remove the logic for null descriptions, since we will
    // always have a description and a source_file.
    description *description;
    int r = m_map.get(fd, &description);
    if (r!=0 || description == NULL) {
        return call_real_ftruncate(fd, length);
    }

    m_table.lock();
    source_file * file = m_table.get(description->get_full_source_name());
    m_table.unlock();
    { int rrr = file->lock_range(length, LLONG_MAX);      assert(rrr == 0); }
    int user_result = call_real_ftruncate(fd, length);
    int e = 0;
    if (user_result==0) {
        if (this->capture_is_enabled()) {
            r = description->truncate(length);
            if (r != 0) {
                e = errno;
                backup_error(e, "failed ftruncate(%d, %ld) while backing up", fd, length);
            }
        }
    } else {
        e = errno; // save errno
    }
    { int rr = file->unlock_range(length, LLONG_MAX);    assert(rr == 0); }
    
    if (user_result!=0) {
        errno = e; // restore errno
    }
    return user_result;
}

///////////////////////////////////////////////////////////////////////////////
//
// truncate() -
//
// Description:
//
//     TBD...
//
void manager::truncate(const char *path, off_t length)
{
    TRACE("entering truncate() with path = ", path);
    // TODO: #6536
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
void manager::mkdir(const char *pathname)
{
    {
        int r = pthread_rwlock_rdlock(&m_session_rwlock);
        if (r!=0) {
            the_manager.fatal_error(r, "failed releasing rwlock at %s:%d", __FILE__, __LINE__);
            return; // don't try to do any more once the lock failed.
        }
    }

    if(m_session != NULL) {
        int r = m_session->capture_mkdir(pathname);
        if (r != 0) {
            the_manager.backup_error(r, "failed mkdir creating %s", pathname);
            // proceed to unlocking below
        }
    }

    {
        int r = pthread_rwlock_unlock(&m_session_rwlock);
        if (r!=0) {
            the_manager.fatal_error(r, "failed releasing rwlock at %s:%d", __FILE__, __LINE__);
        }
    }
            
}


///////////////////////////////////////////////////////////////////////////////
//
void manager::set_throttle(unsigned long bytes_per_second) {
    VALGRIND_HG_DISABLE_CHECKING(&m_throttle, sizeof(m_throttle));
    m_throttle = bytes_per_second;
}

///////////////////////////////////////////////////////////////////////////////
//
unsigned long manager::get_throttle(void) {
    return m_throttle;
}

///////////////////////////////////////////////////////////////////////////////
//
void manager::fatal_error(int errnum, const char *format_string, ...)
{
    va_list ap;
    va_start(ap, format_string);
    
    this->kill();
    this->disable_capture();
    set_error_internal(errnum, format_string, ap);
    va_end(ap);
}

///////////////////////////////////////////////////////////////////////////////
//
void manager::backup_error(int errnum, const char *format_string, ...) {
    va_list ap;
    va_start(ap, format_string);
    
    this->disable_capture();
    set_error_internal(errnum, format_string, ap);
    va_end(ap);
}

///////////////////////////////////////////////////////////////////////////////
//
void manager::set_error_internal(int errnum, const char *format_string, va_list ap) {
    m_backup_is_running = false;
    {
        int r = pthread_mutex_lock(&m_error_mutex);
        if (r!=0) {
            this->kill();  // go ahead and set the error, however
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
        m_an_error_happened = true; // set this last so that it will be OK.
    }
    {
        int r = pthread_mutex_unlock(&m_error_mutex);
        if (r!=0) {
            this->kill();
        }
    }
}


#ifdef GLASSBOX
// Test routines.
void manager::pause_disable(bool pause)
{
    VALGRIND_HG_DISABLE_CHECKING(&m_pause_disable, sizeof(m_pause_disable));
    m_pause_disable = pause;
}

void manager::set_keep_capturing(bool keep_capturing) {
    VALGRIND_HG_DISABLE_CHECKING(&m_keep_capturing, sizeof(m_keep_capturing));
    m_keep_capturing = keep_capturing;
}

bool manager::is_done_copying(void) {
    return m_done_copying;
}
bool manager::is_capturing(void) {
    return m_is_capturing;
}

void manager::set_start_copying(bool start_copying) {
    VALGRIND_HG_DISABLE_CHECKING(&m_start_copying, sizeof(m_start_copying));
    m_start_copying = start_copying;
}
#endif /*GLASSBOX*/
