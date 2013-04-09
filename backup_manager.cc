/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:  
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include "backup_manager.h"
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

pthread_mutex_t backup_manager::m_mutex         = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t backup_manager::m_session_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t backup_manager::m_error_mutex   = PTHREAD_MUTEX_INITIALIZER;
pthread_rwlock_t backup_manager::m_capture_rwlock = PTHREAD_RWLOCK_INITIALIZER;

///////////////////////////////////////////////////////////////////////////////
//
// backup_manager() -
//
// Description: 
//
//     Constructor.
//
backup_manager::backup_manager(void)
    : m_pause_disable(false),
      m_start_copying(true),
      m_keep_capturing(false),
      m_is_capturing(false),
      m_done_copying(false),
      m_is_dead(false),
      m_backup_is_running(false),
      m_capture_enabled(false),
      m_session(NULL),
      m_throttle(ULONG_MAX),
      m_an_error_happened(false),
      m_errnum(BACKUP_SUCCESS),
      m_errstring(NULL)
{
    VALGRIND_HG_DISABLE_CHECKING(&m_backup_is_running, sizeof(m_backup_is_running));
    VALGRIND_HG_DISABLE_CHECKING(&m_is_dead, sizeof(m_is_dead));
    VALGRIND_HG_DISABLE_CHECKING(&m_done_copying, sizeof(m_done_copying));
}

backup_manager::~backup_manager(void) {
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
    int r = 0;
    if (m_is_dead) {
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
    
    pthread_mutex_lock(&m_session_mutex);  // TODO: #6531 handle any errors 
    m_session = new backup_session(source, dest, calls, &m_table, &r);
    pthread_mutex_unlock(&m_session_mutex);   // TODO: #6531 handle any errors
    if (r!=0) {
        goto unlock_out;
    }
    
    r = this->prepare_directories_for_backup(m_session);
    if (r != 0) {
        goto disable_out;
    }

    r = this->turn_on_capture();
    if (r != 0) {
        goto disable_out;
    }

    VALGRIND_HG_DISABLE_CHECKING(&m_is_capturing, sizeof(m_is_capturing));
    m_is_capturing = true;
    m_done_copying = false;

    while (!m_start_copying) sched_yield();

    r = m_session->do_copy();
    if (r != 0) {
        // This means we couldn't start the copy thread (ex: pthread error).
        goto disable_out;
    }

disable_out:
    m_done_copying = true;
    // If the client asked us to keep capturing till they tell us to stop, then do what they said.
    while (m_keep_capturing) sched_yield();

    m_backup_is_running = false;

    {
        int r2 = this->turn_off_capture();
        if (r2 != 0 && r==0) {
            r = r2; // keep going, but remember the error.   Try to disable the descriptoins even if turn_off_capture failed.
        }
    }

    this->disable_descriptions();

    VALGRIND_HG_DISABLE_CHECKING(&m_is_capturing, sizeof(m_is_capturing));
    m_is_capturing = false;

unlock_out:

    pthread_mutex_lock(&m_session_mutex);   // TODO: #6531 handle any errors
    delete m_session;
    m_session = NULL;
    pthread_mutex_unlock(&m_session_mutex);   // TODO: #6531 handle any errors

    {
        int pthread_error = pthread_mutex_unlock(&m_mutex);
        if (pthread_error != 0) {
            m_is_dead = true; // disable backup permanently if the pthread mutexes are failing.
            calls->report_error(pthread_error, "pthread_mutex_unlock failed.  Backup system is probably broken");
            if (r != 0) {
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
    return r;
}

///////////////////////////////////////////////////////////////////////////////
//
int backup_manager::prepare_directories_for_backup(backup_session *session) {
    int r = 0;
    // Loop through all the current file descriptions and prepare them
    // for backup.
    lock_file_descriptor_map(); // TODO: #6532 This lock is much too coarse.  Need to refine it.  This lock deals with a race between file->create() and a close() call from the application.  We aren't using the m_refcount in file_description (which we should be) and we even if we did, the following loop would be racy since m_map.size could change while we are running, and file descriptors could come and go in the meanwhile.  So this really must be fixed properly to refine this lock.
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
        r = open_path(file_name);
        free(file_name);
        if (r != 0) {
            session->abort();
            this->set_error(r, "Failed to open path");
            goto out;
        }

        r = file->create();
        if (r != 0) {
            session->abort();
            this->set_error(r, "Could not create backup file.");
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
    const int size = m_map.size();
    const int middle = size / 2;
    for (int i = 0; i < size; ++i) {
        if (middle == i) {
            TRACE("Pausing on i = ", i); 
            while (m_pause_disable) { sched_yield(); }
            TRACE("Done Pausing on i = ", i);
        }
        file_description *file = m_map.get_unlocked(i);
        if (file == NULL) {
            continue;
        }
        
        file->disable_from_backup();
        //printf("sleeping\n"); usleep(1000);
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

    // Add description to hash table.
    m_table.lock();
    source_file *source = m_table.get(description->get_full_source_name());
    if (source == NULL) {
        TRACE("Creating new source file in hash map ", fd);
        source = new source_file(description->get_full_source_name());
        source->add_reference();
        int r = source->init();
        if (r != 0) {
            source->remove_reference();
            delete source;
            m_table.unlock();
            goto out;
        }

        m_table.put(source);
    }

    m_table.unlock();
    
    pthread_mutex_lock(&m_session_mutex); // TODO: #6531 handle any errors
    
    if (m_session != NULL) {
        char *backup_file_name = m_session->capture_create(file);
        if (backup_file_name != NULL) {
            description->prepare_for_backup(backup_file_name);
            int r = description->create();
            if(r != 0) {
                m_session->abort();
                this->set_error(r, "Could not create backup file");
            }

            free((void*)backup_file_name);
        }
    }
    
    pthread_mutex_unlock(&m_session_mutex); // TODO: #6531  handle any errors
out:
    return;
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

    // Add description to hash table.
    m_table.lock();
    source_file *source = m_table.get(description->get_full_source_name());
    if (source == NULL) {
        TRACE("Creating new source file in hash map ", fd);
        source_file *source = new source_file(description->get_full_source_name());
        source->add_reference();
        int r = source->init();
        if (r != 0) {
            source->remove_reference();
            delete source;
            m_table.unlock();
            goto out;
        }

        m_table.put(source);
    }

    m_table.unlock();
    
    pthread_mutex_lock(&m_session_mutex); // TODO: #6531 handle any errors

    if(m_session != NULL) {
        char *backup_file_name = m_session->capture_open(file);
        if(backup_file_name != NULL) {
            description->prepare_for_backup(backup_file_name);
            int r = description->open();
            if(r != 0) {
                m_session->abort();
                this->set_error(r, "Could not open backup file.");
            }
            
            free((void*)backup_file_name);
        }
    }

    pthread_mutex_unlock(&m_session_mutex); // TODO: #6531 handle any errors

    // TODO: #6533 Remove this dead code.
    oflag++;
 out:
    return;
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
    int r = m_map.erase(fd); // If the fd exists in the map, close it and remove it from the mmap.
    if (r!=0) {
        set_error(r, "failed close(%d) while backing up", fd);
        // TODO: #6534 Shouldn't we abort the backup here?
    }

    file_description * description = m_map.get(fd);
    if (description != NULL) {
        source_file * source = m_table.get(description->get_full_source_name());
        if (source != NULL) {
            m_table.try_to_remove(source);
        }
    }
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
        { int r = pthread_rwlock_rdlock(&m_capture_rwlock); assert(r == 0); }
        { int r = description->lock(); assert(r==0); } // TODO: #6531 Handle any errors
        m_table.lock();
        source_file * file = m_table.get(description->get_full_source_name());
        m_table.unlock();

        // We need the range lock before calling real lock so that the write into the source and backup are atomic wrt other writes.
        TRACE("Grabbing file range lock() with fd = ", fd);
        const uint64_t lock_start = description->get_offset();
        const uint64_t lock_end   = lock_start + nbyte;

        // We want to release the description->lock ASAP, since it's limiting other writes.
        // We cannot release it before the real write since the real write determines the new m_offset.

        file->lock_range(lock_start, lock_end);

        r = call_real_write(fd, buf, nbyte);
        // TODO: #6535 Don't call our write if the first one fails.

        description->increment_offset(r);
        // Now we can release the description lock, since the offset is calculated.
        { int r = description->unlock(); assert(r==0); } // TODO: #6531 Handle any errors

        // We still have the lock range, which we do with pwrite

        if (m_capture_enabled) {
            TRACE("write() captured with fd = ", fd);
            int r = description->pwrite(buf, nbyte, lock_start);
            if (r!=0) {
                set_error(r, "failed write while backing up %s", description->get_full_source_name());
            }
        } 
        TRACE("Releasing file range lock() with fd = ", fd);
        file->unlock_range(lock_start, lock_end);

        { int r = pthread_rwlock_unlock(&m_capture_rwlock); assert(r == 0); }
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
        { int r = description->lock(); assert(r==0); } // TODO: #6531 Handle any errors
        r = call_real_read(fd, buf, nbyte);
        printf("%s:%d r=%ld\n", __FILE__, __LINE__, r);
        if (r>0) {
            description->increment_offset(r); //moves the offset
        }
        {
            int r = description->unlock();
            assert(r==0); // TODO: #6531 Handle any errors
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
ssize_t backup_manager::pwrite(int fd, const void *buf, size_t nbyte, off_t offset)
// Do the write.  If the destination gets a short write, that's an error.
{
    if (m_is_dead || !m_backup_is_running) {
  default_write:
        return  call_real_pwrite(fd, buf, nbyte, offset);
    }
    
    TRACE("entering pwrite() with fd = ", fd);

    file_description *description = NULL;
    description = m_map.get(fd);
    if (description == NULL) {
        goto default_write;
    }

    m_table.lock();
    source_file * file = m_table.get(description->get_full_source_name());
    m_table.unlock();
    { int r = pthread_rwlock_rdlock(&m_capture_rwlock); assert(r == 0); }
    file->lock_range(offset, offset+nbyte);
    ssize_t nbytes_written = call_real_pwrite(fd, buf, nbyte, offset);
    int e = 0;
    if (nbytes_written>0) {
        if (m_capture_enabled) {
            int r = description->pwrite(buf, nbyte, offset);
            if (r!=0) {
                set_error(r, "failed write while backing up %s", description->get_full_source_name());
            }
        }
    } else if (nbytes_written<0) {
        e = errno; // save the errno
    }
    file->unlock_range(offset, offset+nbyte);
    { int r = pthread_rwlock_unlock(&m_capture_rwlock); assert(r == 0); }
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
off_t backup_manager::lseek(int fd, size_t nbyte, int whence) {
    if (m_is_dead) return call_real_lseek(fd, nbyte, whence);
    TRACE("entering seek() with fd = ", fd);
    file_description *description = NULL;
    description = m_map.get(fd);
    if (description == NULL) {
        return call_real_lseek(fd, nbyte, whence);
    } else {
        { int r = description->lock(); assert(r==0); } // TODO: #6531 Handle any errors 
        off_t new_offset = call_real_lseek(fd, nbyte, whence);
        description->lseek(new_offset);
        { int r = description->unlock(); assert(r==0); } // TODO: #6531 Handle any errors
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
int backup_manager::ftruncate(int fd, off_t length)
{
    if (m_is_dead) {
  default_syscall:
        return call_real_ftruncate(fd, length);
    }
    TRACE("entering ftruncate with fd = ", fd);
    file_description *description = m_map.get(fd);
    if (description == NULL) {
        goto default_syscall;
    }

    m_table.lock();
    source_file * file = m_table.get(description->get_full_source_name());
    m_table.unlock();
    { int r = pthread_rwlock_rdlock(&m_capture_rwlock); assert(r == 0); }
    file->lock_range(length, LLONG_MAX);
    int user_result = call_real_ftruncate(fd, length);
    int e = 0;
    if (user_result==0) {
        if (m_capture_enabled) {
            int r = description->truncate(length);
            if (r != 0) {
                e = errno;
                set_error(e, "failed ftruncate(%d, %ld) while backing up", fd, length);
            }
        }
    } else {
        e = errno; // save errno
    }
    file->unlock_range(length, LLONG_MAX);
    { int r = pthread_rwlock_unlock(&m_capture_rwlock); assert(r == 0); }
    
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
void backup_manager::truncate(const char *path, off_t length)
{
    if (m_is_dead) return;
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
void backup_manager::mkdir(const char *pathname)
{
    if (m_is_dead) return;
    pthread_mutex_lock(&m_session_mutex);  // TODO: #6531 handle any errors
    if(m_session != NULL) {
        int r = m_session->capture_mkdir(pathname);
        if (r != 0) {
            set_error(r, "failed mkdir creating %s", pathname);
        }
    }

    pthread_mutex_unlock(&m_session_mutex);  // TODO: #6531 handle any errors
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

///////////////////////////////////////////////////////////////////////////////
//
int backup_manager::turn_on_capture(void) {
    int r = 0;
    r = pthread_rwlock_wrlock(&m_capture_rwlock);
    if (r != 0) {
        goto error_out;
    }

    m_capture_enabled = true;
    r = pthread_rwlock_unlock(&m_capture_rwlock);
    if (r != 0) {
        goto error_out;
    }
    
 error_out:
    return r;
}

///////////////////////////////////////////////////////////////////////////////
//
int backup_manager::turn_off_capture(void) {
    int r = 0;
    r = pthread_rwlock_wrlock(&m_capture_rwlock);
    if (r != 0) {
        goto error_out;
    }

    m_capture_enabled = false;
    r = pthread_rwlock_unlock(&m_capture_rwlock);
    if (r != 0) {
        goto error_out;
    }
    
 error_out:
    return r;
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
void backup_manager::pause_disable(bool pause)
{
    VALGRIND_HG_DISABLE_CHECKING(&m_pause_disable, sizeof(m_pause_disable));
    m_pause_disable = pause;
}

void backup_manager::set_keep_capturing(bool keep_capturing) {
    VALGRIND_HG_DISABLE_CHECKING(&m_keep_capturing, sizeof(m_keep_capturing));
    m_keep_capturing = keep_capturing;
}

bool backup_manager::is_done_copying(void) {
    return m_done_copying;
}
bool backup_manager::is_capturing(void) {
    return m_is_capturing;
}

void backup_manager::set_start_copying(bool start_copying) {
    VALGRIND_HG_DISABLE_CHECKING(&m_start_copying, sizeof(m_start_copying));
    m_start_copying = start_copying;
}
