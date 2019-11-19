/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:  
/*======
This file is part of Percona TokuBackup.

Copyright (c) 2006, 2015, Percona and/or its affiliates. All rights reserved.

    Percona TokuBackup is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License, version 2,
    as published by the Free Software Foundation.

     Percona TokuBackup is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Percona TokuBackup.  If not, see <http://www.gnu.org/licenses/>.

----------------------------------------

    Percona TokuBackup is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License, version 3,
    as published by the Free Software Foundation.

    Percona TokuBackup is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with Percona TokuBackup.  If not, see <http://www.gnu.org/licenses/>.
======= */

#ident "$Id$"

#include "backup_debug.h"
#include "file_hash_table.h"
#include "glassbox.h"
#include "manager.h"
#include "mutex.h"
#include "raii-malloc.h"
#include "real_syscalls.h"
#include "rwlock.h"
#include "source_file.h"
#include "directory_set.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "backup_helgrind.h"

#if defined(DEBUG_HOTBACKUP) && DEBUG_HOTBACKUP
#define WARN(string, arg) HotBackup::CaptureWarn(string, arg)
#define TRACE(string, arg) HotBackup::CaptureTrace(string, arg)
#define ERROR(string, arg) HotBackup::CaptureError(string, arg)
#else
#define WARN(string, arg) 
#define TRACE(string, arg) 
#define ERROR(string, arg) 
#endif

#if defined(PAUSE_POINTS_ON)
#define PAUSE(number) while(HotBackup::should_pause(number)) { sleep(2); } //printf("Resuming from Pause Point.\n");
#else
#define PAUSE(number)
#endif

//////////////////////////////////////////////////////////////////////////////
//
static void print_time(const char *toku_string) throw() {
    time_t t;
    char buf[27];
    time(&t);
    ctime_r(&t, buf);
    fprintf(stderr, "%s %s\n", toku_string, buf);
}

pthread_mutex_t manager::m_mutex         = PTHREAD_MUTEX_INITIALIZER;
pthread_rwlock_t manager::m_session_rwlock = PTHREAD_RWLOCK_INITIALIZER;
pthread_mutex_t manager::m_error_mutex   = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t manager::m_atomic_file_op_mutex = PTHREAD_MUTEX_INITIALIZER;

///////////////////////////////////////////////////////////////////////////////
//
// manager() -
//
// Description: 
//
//     Constructor.
//
manager::manager(void) throw()
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
    TOKUBACKUP_VALGRIND_HG_DISABLE_CHECKING(&m_backup_is_running, sizeof(m_backup_is_running));
#ifdef GLASSBOX
    TOKUBACKUP_VALGRIND_HG_DISABLE_CHECKING(&m_is_capturing, sizeof(m_is_capturing));
    TOKUBACKUP_VALGRIND_HG_DISABLE_CHECKING(&m_done_copying, sizeof(m_done_copying));
    TOKUBACKUP_VALGRIND_HG_DISABLE_CHECKING(&m_an_error_happened, sizeof(m_an_error_happened));
    TOKUBACKUP_VALGRIND_HG_DISABLE_CHECKING(&m_errnum, sizeof(m_errnum));
    TOKUBACKUP_VALGRIND_HG_DISABLE_CHECKING(&m_errstring, sizeof(m_errstring));
#endif
}

manager::~manager(void) throw() {
    if (m_errstring) free(m_errstring);
}

// This is a per-thread variable that indicates if we are the thread that can do the backup calls directly (and if so, here they are).
// If NULL then any errors must be saved using set_error_internal.
// The functions fatal_error and backup_error handle all of this. 
// Except for a few calls right at the top of the tree, everything should use fatal_error and backup_error.
__thread backup_callbacks *thread_has_backup_calls = NULL;


///////////////////////////////////////////////////////////////////////////////
//
// do_backup() -
//
// Description: 
//
//     
//
int manager::do_backup(directory_set *dirs, backup_callbacks *calls) throw() {
    thread_has_backup_calls = calls;

    int r = 0;
    if (this->is_dead()) {
        r = EINVAL;
        backup_error(r, "Backup system is dead");
        goto error_out;
    }
    m_an_error_happened = false;
    m_backup_is_running = true;
    r = calls->poll(0, "Preparing backup");
    if (r != 0) {
        backup_error(r, "User aborted backup");
        goto error_out;
    }

    r = pthread_mutex_trylock(&m_mutex);
    if (r != 0) {
        if (r==EBUSY) {
            backup_error(r, "Another backup is in progress.");
            goto error_out;
        }
        fatal_error(r, "mutex_trylock at %s:%d", __FILE__, __LINE__);
        goto error_out;
    }

    r = dirs->validate();
    if (r != 0) {
        goto unlock_out;
    }

    {
        with_rwlock_wrlocked ms(&m_session_rwlock, BACKTRACE(NULL));

        {
            with_mutex_locked mt(&copier::m_todo_mutex, BACKTRACE(NULL));
            m_session = new backup_session(dirs, calls, &m_table);
        }
        print_time("Toku Hot Backup: Started:");    

        r = this->prepare_directories_for_backup(m_session, BACKTRACE(NULL));
        if (r != 0) {
            // RAII saves the day.  We weren't unlocking the mutex properly.
            goto disable_out;
        }

        this->enable_capture();
        this->enable_copy();
    }

    WHEN_GLASSBOX( ({
                m_done_copying = false;
                m_is_capturing = true;
                while (!m_start_copying) sched_yield();
            }) );

    r = m_session->do_copy();
    if (r != 0) {
        this->backup_error(r, "COPY phase returned an error: %d", r);
        // This means we couldn't start the copy thread (ex: pthread error).
        goto disable_out;
    }

disable_out: // preserves r if r!=0

    WHEN_GLASSBOX( ({
        m_done_copying = true;
        // If the client asked us to keep capturing till they tell us to stop, then do what they said.
        while (m_keep_capturing) sched_yield();
            }) );

    calls->before_stop_capt_call();
    {
        with_rwlock_wrlocked ms(&m_session_rwlock, BACKTRACE(NULL));

        m_backup_is_running = false;
        this->disable_capture();
        this->disable_descriptions();
        WHEN_GLASSBOX(m_is_capturing = false);
        print_time("Toku Hot Backup: Finished:");
        // We need to remove any extra renamed files that may have made it
        // to the backup session just after copy finished.
        m_session->cleanup();
        delete m_session;
        m_session = NULL;
    }
    calls->after_stop_capt_call();

unlock_out: // preserves r if r!0

    pmutex_unlock(&m_mutex, BACKTRACE(NULL));
    if (m_an_error_happened) {
        calls->report_error(m_errnum, m_errstring);
        if (r==0) {
            r = m_errnum; // if we already got an error then keep it.
        }
    }

error_out:
    thread_has_backup_calls = NULL;
    return r;
}

///////////////////////////////////////////////////////////////////////////////
//
int manager::prepare_directories_for_backup(backup_session *session, backtrace bt) throw() {
    int r = 0;
    // Loop through all the current file descriptions and prepare them
    // for backup.
    with_fmap_locked fm(BACKTRACE(&bt)); // TODO: #6532 This lock is much too coarse.  Need to refine it.  This lock deals with a race between file->create() and a close() call from the application.  We aren't using the m_refcount in file_description (which we should be) and we even if we did, the following loop would be racy since m_map.size could change while we are running, and file descriptors could come and go in the meanwhile.  So this really must be fixed properly to refine this lock.
    for (int i = 0; i < m_map.size(); ++i) {
        description *file = m_map.get_unlocked(i);
        if (file == NULL) {
            continue;
        }
        
        source_file * source = file->get_source_file();
        with_file_hash_table_mutex mtl(&m_table); // We think this fixes #34.  Also this must before the source_file_name_read_lock.
        with_source_file_name_read_lock sfl(source);

        if (!session->is_prefix_of_realpath(source->name())) {
            continue;
        }

        with_object_to_free<char*> file_name(session->translate_prefix_of_realpath(source->name()));
        if(session->file_is_excluded(file_name.value)) {
            goto out;
        }

        r = open_path(file_name.value);
        if (r != 0) {
            backup_error(r, "Failed to open path");
            goto out;
        }

        r = source->try_to_create_destination_file(file_name.value);
        if (r != 0) {
            backup_error(r, "Could not create backup file.");
            // Don't need to free file_name, since try_to_create_destination_file takes ownership (having deleted it if an error happened).
            goto out;
        }

    }
    
out:
    return r;
}

///////////////////////////////////////////////////////////////////////////////
//
void manager::disable_descriptions(void) throw() {
    with_fmap_locked ml(BACKTRACE(NULL));
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

        source_file * source = file->get_source_file();
        if (source != NULL) {
            source->try_to_remove_destination();
        }
    }

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
int manager::open(int fd, const char *file, int flags) throw() {
    TRACE("entering open() with fd = ", fd);
    // Skip the given 'file' if it is not a regular file.
    struct stat buf;
    int stat_r = fstat(fd, &buf);
    if (stat_r != 0) {
        stat_r = errno;
        this->backup_error(stat_r, "Could not stat incoming file %s", file);
    } else if (!S_ISREG(buf.st_mode)){
        // We don't care about tracking non-regular files, just return.
        return 0;
    }

    // Create the description and source file objects.  This happens
    // whether we a backup session is in progress or not.
    int result = this->setup_description_and_source_file(fd, file, flags);
    if (result != 0) {
        // All errors in above function are fatal, just return, it's over.
        return result;
    }

    // If there is no session, or we can't lock it, just return.
    with_manager_enter_session_and_lock msl(this);
    if (!msl.entered) {
        return 0;
    }

    PAUSE(HotBackup::CAPTURE_OPEN);

    // Since there is an active backup session, we need to create the
    // destination file object.  This will also create the actual
    // backup copy of the corresponding file in the user's source
    // directory.
    description * description = NULL;
    source_file * source = NULL;
    char * backup_file_name = NULL;
    // First, get the description and source_file objects associated
    // with the given fd.
    m_map.get(fd, &description, BACKTRACE(NULL));
    
    source = description->get_source_file();
    with_source_file_name_read_lock sfl(source);

    // Next, determine the full path of the backup file.
    result = m_session->capture_open(file, &backup_file_name);
    if (result != 0) {
        goto out;
    }
    
    // Finally, create the backup file and destination_file object.
    if (backup_file_name != NULL) {
        result = source->try_to_create_destination_file(backup_file_name);
        if (result != 0) {
            backup_error(result, "Could not open backup file %s", backup_file_name);
            goto out;
        }
        free((void*)backup_file_name);
    }

out:
    return result;
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
    description * file = NULL;
    m_map.get(fd, &file, BACKTRACE(NULL));
    
    if (file == NULL) {
        return;
    }
    
    source_file * source = file->get_source_file();
    {
        with_manager_enter_session_and_lock msl(this);
        if (msl.entered) {
            with_file_hash_table_mutex mtl(&m_table);
            source->try_to_remove_destination();
        }
    }

    int r1 = m_map.erase(fd, BACKTRACE(NULL)); // If the fd exists in the map, close it and remove it from the mmap.
    // This will remove both the source file and destination file
    // objects, if they exist.
    m_table.try_to_remove_locked(source);
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
ssize_t manager::write(int fd, const void *buf, size_t nbyte) throw() {
    TRACE("entering write() with fd = ", fd);
    bool ok = true;
    description *description;
    if (ok) {
        m_map.get(fd, &description, BACKTRACE(NULL));
        if (description == NULL) ok = false;
    }
    bool have_description_lock = false;
    if (ok && description) {
        description->lock(BACKTRACE(NULL));
        have_description_lock = true;
    }
    source_file *file = NULL;
    bool have_range_lock = false;
    uint64_t lock_start=0, lock_end=0;
    if (ok && description) {
        file = description->get_source_file();
        // We need the range lock before calling real lock so that the write into the source and backup are atomic wrt other writes.
        TRACE("Grabbing file range lock() with fd = ", fd);
        lock_start = description->get_offset();
        lock_end   = lock_start + nbyte;

        // We want to release the description->lock ASAP, since it's limiting other writes.
        // We cannot release it before the real write since the real write determines the new m_offset.
        file->lock_range(lock_start, lock_end);
        have_range_lock = true;
    }
    ssize_t n_wrote = call_real_write(fd, buf, nbyte);
    if (n_wrote>0 && description) { // Don't need OK, just need description
        // actually wrote something.
        description->increment_offset(n_wrote);
    }
    // Now we can release the description lock, since the offset is calculated.  Release it even if not OK.
    if (have_description_lock) {
        description->unlock(BACKTRACE(NULL));
    }

    // We still have the lock range, with which we do the pwrite.
    if (ok) {
        with_manager_enter_session_and_lock msl(this);
        if (msl.entered) {
            TRACE("write() captured with fd = ", fd);
            destination_file * dest_file = file->get_destination();
            if (dest_file != NULL) {
                int r = dest_file->pwrite(buf, nbyte, lock_start);
                if (r!=0) {
                    // The error has been reported.
                    ok = false;
                }
            }
        }
    }

    if (have_range_lock) { // Release the range lock if if not OK.
        TRACE("Releasing file range lock() with fd = ", fd);
        int r = file->unlock_range(lock_start, lock_end);
        // The error has been reported.
        if (r!=0) ok = false;
    }
    return n_wrote;
}

///////////////////////////////////////////////////////////////////////////////
//
// read() -
//
// Description:
//
//     Do the read.
//
ssize_t manager::read(int fd, void *buf, size_t nbyte) throw() {
    TRACE("entering write() with fd = ", fd);
    ssize_t r = 0;
    description *description;
    m_map.get(fd, &description, BACKTRACE(NULL));
    if (description == NULL) {
        r = call_real_read(fd, buf, nbyte);
    } else {
        description->lock(BACKTRACE(NULL));
        r = call_real_read(fd, buf, nbyte);
        if (r>0) {
            description->increment_offset(r); //moves the offset
        }
        description->unlock(BACKTRACE(NULL));
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
ssize_t manager::pwrite(int fd, const void *buf, size_t nbyte, off_t offset) throw()
// Do the write, returning the number of bytes written.
// Note: If the backup destination gets a short write, that's an error.
{
    TRACE("entering pwrite() with fd = ", fd);
    description *description;
    {
        m_map.get(fd, &description, BACKTRACE(NULL));
        if (description == NULL) {
            int res = call_real_pwrite(fd, buf, nbyte, offset);
            return res;
        }
    }

    source_file * file = description->get_source_file();

    file->lock_range(offset, offset+nbyte);
    ssize_t nbytes_written = call_real_pwrite(fd, buf, nbyte, offset);
    int e = 0;
    if (nbytes_written>0) {
        with_manager_enter_session_and_lock msl(this);
        if (msl.entered) {
            destination_file * dest_file = file->get_destination();
            if (dest_file != NULL) {
                ignore(dest_file->pwrite(buf, nbyte, offset)); // nothing more to do.  It's been reported.
            }
        }
    } else if (nbytes_written<0) {
        e = errno; // save the errno
    }

    ignore(file->unlock_range(offset, offset+nbyte)); // nothing more to do.  It's been reported.
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
off_t manager::lseek(int fd, size_t nbyte, int whence) throw() {
    TRACE("entering seek() with fd = ", fd);
    description *description;
    bool ok = true;
    {
        m_map.get(fd, &description, BACKTRACE(NULL));
        if (description==NULL) ok = false;
    }
    if (ok) {
        description->lock(BACKTRACE(NULL));
    }
    off_t new_offset = call_real_lseek(fd, nbyte, whence);
    if (ok) {
        description->lseek(new_offset);
        description->unlock(BACKTRACE(NULL));
    }
    return new_offset;
}

///////////////////////////////////////////////////////////////////////////////
//
// rename() -
//
// Description:
//
//     TBD...
//
int manager::rename(const char *oldpath, const char *newpath) throw() {
    TRACE("entering rename() with oldpath = ", oldpath);
    int user_error = 0;
    const char * full_old_path = call_real_realpath(oldpath, NULL);
    if (full_old_path == NULL) {
        int error = errno;
        if (error == ENOMEM) {
            the_manager.backup_error(error, "Could not rename file.");
        }

        return call_real_rename(oldpath, newpath);
    }

    // We need the newpath to exist to finish our own rename work.
    // So just call it now, regardless of CAPTURE state.
    user_error = call_real_rename(oldpath, newpath);
    if (user_error == 0) {
        with_manager_enter_session_and_lock msl(this);
        if (msl.entered) {
            this->capture_rename(full_old_path, newpath); // takes ownership of the full_old_path, so tough to make RAII.
        }
    }

    free(const_cast<char*>(full_old_path));
    TRACE("rename() exiting...", oldpath);
    return user_error;
}

///////////////////////////////////////////////////////////////////////////////
//
void manager::capture_rename(const char * full_old_path, const char * newpath)
{
    int error = 0;
    int r = 0;

    // Get the full path of the recently renanmed file.
    // We could not call this earlier, since the new file path
    // did not exist till AFTER we called the real rename on 
    // the original source file.
    with_object_to_free<char*> full_new_path(call_real_realpath(newpath, NULL));
    if (full_new_path.value != NULL) {
        TRACE("renaming backup copy", full_new_path.value);
        bool original_present = m_session->is_prefix_of_realpath(full_old_path);
        bool new_present = m_session->is_prefix_of_realpath(full_new_path.value);

        // Four cases:
        if ((original_present == false) && (new_present == false)) { 
            // 1. If neither file is in the directory we just bail.

        } else if (original_present == true && new_present == false) {
            // 2. If only original path in directory: We need to
            // REMOVE the file from the backup if it has been created.
            // We also need try to REMOVE the source file from the
            // todo list in the copier.
            
        } else if (original_present == false && new_present == true) {
            // 3. Only new path: we have to ADD the new file using the
            // create path.  We also need to add it to the copier's
            // toto list.
            
        } else {
            // 4. Both in source directory.
            // Get the new full paths of the destination file.
            // NOTE: This string will be owned by the destination file object.
            with_object_to_free<char*> full_old_destination_path(m_session->translate_prefix_of_realpath(full_old_path));
            with_object_to_free<char*> full_new_destination_path(m_session->translate_prefix_of_realpath(full_new_path.value));

            // If we got to this point, we have called rename on the
            // source file itself.  So we must update our source_file
            // object with the new name.  We must also update the
            // destination_file object because we are inside of a
            // session.
            r = m_table.rename_locked(full_old_path, 
                                      full_new_path.value,
                                      full_old_destination_path.value,
                                      full_new_destination_path.value);

            if (r != 0) {
                // Nothing.  The error has been reported in rename_locked.
            } else {
                // If the copier has already copied or is copying the
                // file, this will succceed.  If the copier has not yet
                // created the file this will fail, and it should find it
                // in its todo list.  However, if we want to be sure that
                // the new name is in its todo list, we must add it to the
                // todo list ourselves, just to be sure that it is copied.

                // NOTE: If the orignal file name is still in our todo
                // list, the copier will attempt to copy it, but since
                // it has already been renamed it will fail with
                // ENOENT, which we ignore in COPY, and move on to the
                // next item in the todo list.  Add to the copier's
                // todo list.
                m_session->add_to_copy_todo_list(full_new_destination_path.value);
            }
        }
    } else {
        error = errno;
        if (error == ENOMEM) {
            the_manager.backup_error(error, "Could not rename user file: %s", newpath);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
//
// unlink() -
//
//
int manager::unlink(const char *path) throw() {
    TRACE("entering unlink with path = ", path);
    int r = 0;
    int user_error = 0;
    source_file * source = NULL;
    with_object_to_free<char*> full_path(call_real_realpath(path, NULL));
    if (full_path.value == NULL) {
        int error = errno;
        if (error == ENOMEM) {
            the_manager.backup_error(error, "Could not unlink path.");
        }
        user_error = call_real_unlink(path);
        return user_error;
    }

    // We have to call unlink on the source file AFTER we have
    // resolved the given path to the full path.  Otherwise,
    // call_real_realpath() will fail saying it cannot find the path.
    user_error = call_real_unlink(path);
    if (user_error != 0) {
        goto free_out;
    }

    m_table.get_or_create_locked(full_path.value, &source);

    {
        with_rwlock_rdlocked ms(&m_session_rwlock);
        with_file_hash_table_mutex mtl(&m_table);

        if (this->should_capture_unlink_of_file(full_path.value)) {
            // 1. Find source file, unlink it.
            // 2. Get destination file from source file.
            destination_file * dest = source->get_destination();
            if (dest == NULL) {
                // The destination object may not exist in one of three cases:
                // 1.  The copier hasn't yet gotten to copying the file.
                // 2.  The copier has finished copying the file.
                // 3.  There is no open fd associated with this file.
                with_object_to_free<char*> dest_path(m_session->translate_prefix_of_realpath(full_path.value));
                r = source->try_to_create_destination_file(dest_path.value);
                if (r != 0) {
                    // RAII to the rescue.  Forgot to release the session_rwlock.
                    goto free_out;
                }

                dest = source->get_destination();
            }

            // 3. Unlink the destination file.
            r = dest->unlink();
            if (r != 0) {
                int error = errno;
                this->backup_error(error, "Could not unlink backup copy.");
            }
        
            // If it does not exist, and if backup is running,
            // it may be in the todo list. Since we have the hash 
            // table lock, the copier can't add it, and rename() threads
            // can't alter the name and re-hash it till we are done.
        }

        // We have to unlink the source file, regardless of whether ther
        // is a backup session in progress or not.
        {
            with_source_file_name_write_lock sfl(source);
            source->unlink();
            source->try_to_remove_destination();
        }

        m_table.try_to_remove(source);
    }

free_out:
    return user_error;
}

///////////////////////////////////////////////////////////////////////////////
//
// ftruncate() -
//
// Description:
//
//     TBD...
//
int manager::ftruncate(int fd, off_t length) throw() {
    TRACE("entering ftruncate with fd = ", fd);
    // TODO: Remove the logic for null descriptions, since we will
    // always have a description and a source_file.
    description *description;
    {
        m_map.get(fd, &description, BACKTRACE(NULL));
        if (description == NULL) {
            int res = call_real_ftruncate(fd, length);
            return res;
        }
    }

    source_file * file = description->get_source_file();

    file->lock_range(length, LLONG_MAX);
    int user_result = call_real_ftruncate(fd, length);
    int e = 0;
    if (user_result==0) {
        with_manager_enter_session_and_lock msl(this);
        if (msl.entered) {
            destination_file * dest_file = file->get_destination();
            if (dest_file != NULL) {
                 // the error from truncate been reported, so there's
                 // nothing we can do about that error except to try
                 // to unlock the range.
                ignore(dest_file->truncate(length));
            }
        }
    } else {
        e = errno; // save errno
    }
    ignore(file->unlock_range(length, LLONG_MAX)); // it's been reported, so there's not much more to do
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
int manager::truncate(const char *path, off_t length) throw() {
    int r;
    int user_error = 0;
    int error = 0;
    with_object_to_free<char*> full_path(call_real_realpath(path, NULL));
    if (full_path.value == NULL) {
        error = errno;
        the_manager.backup_error(error, "Failed to truncate backup file.");
        return call_real_truncate(path, length);
    }

    with_rwlock_rdlocked ms(&m_session_rwlock);
    
    if (m_session != NULL && m_session->is_prefix_of_realpath(full_path.value)) {
        with_object_to_free<char *> destination_file(m_session->translate_prefix_of_realpath(full_path.value));
        // Find and lock the associated source file.
        source_file *file;
        {
            with_file_hash_table_mutex mtl(&m_table);

            file = m_table.get(destination_file.value);
            file->add_reference();
        }
        
        file->lock_range(length, LLONG_MAX);
        
        user_error = call_real_truncate(full_path.value, length);
        if (user_error == 0 && this->capture_is_enabled()) {
            r = call_real_truncate(destination_file.value, length);
            if (r != 0) {
                error = errno;
                if (error != ENOENT) {
                    the_manager.backup_error(error, "Could not truncate backup file.");
                }
            }
        }

        r = file->unlock_range(length, LLONG_MAX);
        file->remove_reference();
        if (r != 0) {
            user_error = call_real_truncate(path, length);
            // More RAII-fixed problems (the session rwlock wasn't freed, and the destination_file wasn't freed.
            goto free_out;
        }
    } else {
        // No backup is in progress, just truncate the source file.
        user_error = call_real_truncate(path, length);
        if (user_error != 0) {
            // More RAII-fixed problems (the session rwlock wasn't freed)
            goto free_out;
        }
    }

free_out:
    return user_error;

}

///////////////////////////////////////////////////////////////////////////////
//
// mkdir() -
//
// Description:
//
//     TBD...
//
void manager::mkdir(const char *pathname) throw() {
    with_rwlock_rdlocked ml(&m_session_rwlock);

    if(m_session != NULL) {
        int r = m_session->capture_mkdir(pathname);
        if (r != 0) {
            the_manager.backup_error(r, "failed mkdir creating %s", pathname);
            // proceed to unlocking below
        }
    }
}


///////////////////////////////////////////////////////////////////////////////
//
void manager::set_throttle(unsigned long bytes_per_second) throw() {
    TOKUBACKUP_VALGRIND_HG_DISABLE_CHECKING(&m_throttle, sizeof(m_throttle));
    m_throttle = bytes_per_second;
}

///////////////////////////////////////////////////////////////////////////////
//
unsigned long manager::get_throttle(void) const throw() {
    return m_throttle;
}

void manager::backup_error_ap(int errnum, const char *format_string, va_list ap) throw() {
    this->disable_capture();
    this->disable_copy();
    if (thread_has_backup_calls) {
        int len = 2*PATH_MAX + strlen(format_string) + 1000;
        with_malloced<char*> string(len);
        int nwrote = vsnprintf(string.value, len, format_string, ap);
        snprintf(string.value+nwrote, len-nwrote, "  error %d (%s)", errnum, strerror(errnum));
        thread_has_backup_calls->report_error(errnum, string.value);
    } else {
        set_error_internal(errnum, format_string, ap);
    }
}

///////////////////////////////////////////////////////////////////////////////
//
void manager::fatal_error(int errnum, const char *format_string, ...) throw() {
    va_list ap;
    va_start(ap, format_string);
    this->kill();
    this->backup_error_ap(errnum, format_string, ap);
    va_end(ap);
}

///////////////////////////////////////////////////////////////////////////////
//
void manager::backup_error(int errnum, const char *format_string, ...)  throw() {
    va_list ap;
    va_start(ap, format_string);
    this->backup_error_ap(errnum, format_string, ap);
    va_end(ap);
}

///////////////////////////////////////////////////////////////////////////////
//
void manager::set_error_internal(int errnum, const char *format_string, va_list ap) throw() {
    m_backup_is_running = false;
    pmutex_lock(&m_error_mutex, BACKTRACE(NULL));
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
    pmutex_unlock(&m_error_mutex, BACKTRACE(NULL));
}

///////////////////////////////////////////////////////////////////////////////
//
bool manager::try_to_enter_session_and_lock(void) throw() {
    prwlock_rdlock(&m_session_rwlock);

    if (m_session == NULL) {
        prwlock_unlock(&m_session_rwlock);
        return false;
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////
//
void manager::exit_session_and_unlock_or_die(void) throw() {
    prwlock_unlock(&m_session_rwlock);
}

///////////////////////////////////////////////////////////////////////////////
//
int manager::setup_description_and_source_file(int fd, const char *file, const int flags) throw() {
    int error = 0;
    source_file * source = NULL;
    description * file_description = NULL;
    
    // Resolve the given, possibly relative, file path to
    // the full path. 
    {
        with_object_to_free<char*> full_source_file_path(call_real_realpath(file, NULL));
        if (full_source_file_path.value == NULL) {
            error = errno;
            // This error is not recoverable, because we can't guarantee 
            // that we can CAPTURE any calls on the given fd or file.
            this->backup_error(errno, "realpath failed on %s", file);
            goto error_out;
        }
        
        m_table.get_or_create_locked(full_source_file_path.value, &source, flags);
    }
    
    // Now that we have the source file, regardless of whether we had
    // to create it or not, we can now create the file description
    // object that will track the offsets and map this fd with the
    // source file object.
    file_description = new description();
    file_description->set_source_file(source);
    m_map.put(fd, file_description);

 error_out:
    return error;
}

void manager::lock_file_op(void)
{
    pmutex_lock(&m_atomic_file_op_mutex);
}

void manager::unlock_file_op(void)
{
    pmutex_unlock(&m_atomic_file_op_mutex);
}

bool manager::should_capture_unlink_of_file(const char *file) throw() {
    if (m_session != NULL &&
        this->capture_is_enabled() &&
        m_session->is_prefix_of_realpath(file) &&
        !m_session->file_is_excluded(file)) {
        return true;
    }

    return false;
}

#ifdef GLASSBOX
// Test routines.
void manager::pause_disable(bool pause) throw() {
    TOKUBACKUP_VALGRIND_HG_DISABLE_CHECKING(&m_pause_disable, sizeof(m_pause_disable));
    m_pause_disable = pause;
}

void manager::set_keep_capturing(bool keep_capturing) throw() {
    TOKUBACKUP_VALGRIND_HG_DISABLE_CHECKING(&m_keep_capturing, sizeof(m_keep_capturing));
    m_keep_capturing = keep_capturing;
}

bool manager::is_done_copying(void) throw() {
    return m_done_copying;
}
bool manager::is_capturing(void) throw() {
    return m_is_capturing;
}

void manager::set_start_copying(bool start_copying) throw() {
    TOKUBACKUP_VALGRIND_HG_DISABLE_CHECKING(&m_start_copying, sizeof(m_start_copying));
    m_start_copying = start_copying;
}
#endif /*GLASSBOX*/
