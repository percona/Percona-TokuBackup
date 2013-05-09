/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include "backup_debug.h"
#include "copier.h"
#include "file_hash_table.h"
#include "manager.h"
#include "mutex.h"
#include "real_syscalls.h"
#include "source_file.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <vector>

template class std::vector<char *>;

#if DEBUG_HOTBACKUP
#define WARN(string, arg) HotBackup::CopyWarn(string, arg)
#define TRACE(string, arg) HotBackup::CopyTrace(string, arg)
#define ERROR(string, arg) HotBackup::CopyError(string, arg)
#else
#define WARN(string, arg) 
#define TRACE(string, arg) 
#define ERROR(string, arg) 
#endif

#if PAUSE_POINTS_ON
#define PAUSE(int) while(HotBackup::should_pause(int)) { sched_yield(); }
#else
#define PAUSE(int)
#endif

////////////////////////////////////////////////////////////////////////////////
//
// is_dot() -
//
// Description: 
//
//     Helper function that returns true if the given directory 
// entry is either of the special cases: ".." or ".".
//
static bool is_dot(struct dirent const *entry)
{
    if (strcmp(entry->d_name, "..") == 0 || strcmp(entry->d_name, ".") == 0) {
        return true;
    }
    return false;
}

pthread_mutex_t copier::m_todo_mutex = PTHREAD_MUTEX_INITIALIZER;

////////////////////////////////////////////////////////////////////////////////
//
// copier() - 
//
// Description:
//
//     Constructor for this copier object.
//
copier::copier(backup_callbacks *calls, file_hash_table * const table)
    : m_source(NULL), 
      m_dest(NULL), 
      m_calls(calls), 
      m_table(table),
      m_total_bytes_backed_up(0),
      m_total_files_backed_up(0)
{}

////////////////////////////////////////////////////////////////////////////////
//
// set_directories() -
//
// Description: 
//
//     Adds a directory heirarchy to be copied from the given source
// to the given destination.
//
void copier::set_directories(const char *source, const char *dest)
{
    m_source = source;
    m_dest = dest;
}

////////////////////////////////////////////////////////////////////////////////
//
// start_copy() -
//
// Description: 
//
//     Loops through all files and subdirectories of the current 
// directory that has been selected for backup.
//
int copier::do_copy(void) {
    int r = 0;
    char *fname = 0;
    size_t n_known = 0;
    pmutex_lock(&m_todo_mutex);
    // Start with "."
    m_todo.push_back(strdup("."));
    n_known = m_todo.size();
    pmutex_unlock(&m_todo_mutex);
    while (n_known != 0) {

        if (!the_manager.copy_is_enabled()) goto out;
        
        pmutex_lock(&m_todo_mutex);
        fname = m_todo.back();
        pmutex_unlock(&m_todo_mutex);
        TRACE("Copying: ", fname);
        
        char *msg = malloc_snprintf(strlen(fname)+100, "Backup progress %ld bytes, %ld files.  %ld files known of. Copying file %s",  m_total_bytes_backed_up, m_total_files_backed_up, n_known, fname);
        // Use n_done/n_files.   We need to do a better estimate involving n_bytes_copied/n_bytes_total
        // This one is very wrongu
        r = m_calls->poll(0, msg);
        free(msg);
        if (r != 0) {
            goto out;
        }

        pmutex_lock(&m_todo_mutex);
        m_todo.pop_back();
        pmutex_unlock(&m_todo_mutex);
        
        r = this->copy_stripped_file(fname);
        free((void*)fname);
        fname = NULL;
        if(r != 0) {
            goto out;
        }
        m_total_files_backed_up++;
        
        pmutex_lock(&m_todo_mutex);
        n_known = m_todo.size();
        pmutex_unlock(&m_todo_mutex);
    }

out:
    this->cleanup();
    return r;
}


static void pathcat(char *dest, size_t destlen, const char *a, int alen, const char *b)
// Effect: Concatenate paths A and B (insert a / between if needed) into dest.  If a ends with a / and b starts with a / then put only 1 / in.
// Requires: destlen is big enough.  A is nonempty.
{
    bool a_has_slash_at_end = (alen>0) && (a[alen-1]=='/');
    if (b[0]=='/') b++;
    int r = snprintf(dest, destlen, "%s%s%s", a, a_has_slash_at_end ? "" : "/", b);
    if (r>=(int)destlen) {
        static bool ecount=0;
        ecount++;
        if (ecount==0) fprintf(stderr, "pathcat length computation error in backup\n");
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// copy_stripped_file() -
//
// Description: 
//
//     Copy's the given file, using this copier object's source and
// destination directory members to determine the exact location
// of the file in both the original and backup locations.
//
int copier::copy_stripped_file(const char *file) {
    int r = 0;
    bool is_dot = (strcmp(file, ".") == 0);
    if (is_dot) {
        // Just copy the root of the backup tree.
        r = this->copy_full_path(m_source, m_dest, "");
        if (r != 0) {
            goto out;
        }
    } else {
        // Prepend the source directory path to the file name.
        int m_source_len = strlen(m_source);
        int slen = m_source_len + strlen(file) + 2;
        char full_source_file_path[slen];
        pathcat(full_source_file_path, slen, m_source, m_source_len, file);

        // Prepend the destination directory path to the file name.
        int m_dest_len = strlen(m_dest);
        int dlen = m_dest_len + strlen(file) + 2;
        char full_dest_file_path[dlen];
        pathcat(full_dest_file_path, dlen, m_dest, m_dest_len, file);
        
        r = this->copy_full_path(full_source_file_path, full_dest_file_path, file);
        if(r != 0) {
            goto out;
        }
    }

out:
    return r;
}


////////////////////////////////////////////////////////////////////////////////
//
// copy_full_path() - 
//
// Description: 
//
//     Copies the given source file, or directory, to our backup
// directory, using the given source and destination prefixes to
// determine the relative location of the file in the directory
// heirarchy.
//
int copier::copy_full_path(const char *source, const char* dest, const char *file) {
    int r = 0;
    struct stat sbuf;
    int stat_r = stat(source, &sbuf);
    if (stat_r != 0) {
        stat_r = errno;
        // Ignore errors about file not existing, 
        // because we have not yet opened the file with open(), 
        // which would prevent it from disappearing.
        if (stat_r == ENOENT) {
            goto out;
        }

        r = stat_r;
        char *string = malloc_snprintf(strlen(dest)+100, "error stat(\"%s\"), errno=%d (%s) at %s:%d", dest, r, strerror(r), __FILE__, __LINE__);
        m_calls->report_error(errno, string);
        free(string);
        goto out;
    }
    
    // See if the source path is a directory or a real file.
    if (S_ISREG(sbuf.st_mode)) {
        r = this->copy_regular_file(source, dest, sbuf.st_size);
        if (r != 0) {
            // The error should already have been reported, so we simply return r.
            goto out;
        }
    } else if (S_ISDIR(sbuf.st_mode)) {
        // Open the directory to be copied (source directory, full path).
        DIR *dir = opendir(source);
        if(dir == NULL) {
            r = errno;
            // If the directory was deleted from underneath us, just skip it.
            if (r != ENOENT) { 
                goto out;
            }
        }

        // Make the directory in the backup destination.
        r = call_real_mkdir(dest, 0777);
        if (r < 0) {
            int mkdir_errno = errno;
            if(mkdir_errno != EEXIST) {
                char *string = malloc_snprintf(strlen(dest)+100, "error mkdir(\"%s\"), errno=%d (%s) at %s:%d", dest, mkdir_errno, strerror(mkdir_errno), __FILE__, __LINE__);
                m_calls->report_error(mkdir_errno, string);
                free(string);
                r = mkdir_errno;
                closedir(dir); // ignore errors from this.
                goto out;
            }
            
            ERROR("Cannot create directory that already exists = ", dest);
        }

        r = this->add_dir_entries_to_todo(dir, file);
        if (r != 0) {
            closedir(dir); // ignore errors from this.
            goto out;
        }

        r = closedir(dir);
        if (r!=0) {
            r = errno;
            the_manager.backup_error(r, "Cannot close dir %s during backup at %s:%d\n", source, __FILE__, __LINE__);
            goto out;
        }
    } else {
        // TODO: #6538 Do we need to add a case for hard links?
        if (S_ISLNK(sbuf.st_mode)) {
            WARN("Link file found, but not copied:", file);
        }
    }
    
out:
    return r;
}

////////////////////////////////////////////////////////////////////////////////
//
// copy_regular_file() - 
//
// Description: 
//
//     Using the given full paths to both the original file and the
// intended path to the backup copy of aforementioned file, this
// function creates the new file, then copies all the bytes from
// one to the other.
//
int copier::copy_regular_file(const char *source, const char *dest, off_t source_file_size)
{
    int srcfd = call_real_open(source, O_RDONLY);
    if (srcfd < 0) {
        int error = errno;
        if (error == ENOENT) {
            return 0;
        } else {
            the_manager.backup_error(error, "Could not open source file: %s", source);
            return error;
        }
    }

    source_info src_info = {srcfd, source, source_file_size, NULL};
    int result = this->copy_using_source_info(src_info, dest);
    
    int r = call_real_close(srcfd);
    if (r != 0) {
        r = errno;
        the_manager.backup_error(r, "Could not close %s at %s:%d", source, __FILE__, __LINE__);
        return r;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////
//
int copier::copy_using_source_info(source_info src_info, const char *path)
{
    source_file * file = NULL;
    TRACE("Creating new source file", path);
    int result = m_table->get_or_create_locked(src_info.m_path, &file);
    if (result != 0) {
        return result;
    }

    src_info.m_file = file;
    result = this->create_destination_and_copy(src_info, path);

    m_table->try_to_remove_locked(file);
    return result;
}

////////////////////////////////////////////////////////////////////////////////
//
int copier::create_destination_and_copy(source_info src_info,  const char *path)
{
    char * dest_path = strdup(path);
    if (dest_path == NULL) {
        int r = errno;
        return r;
    }

    // Try to create the destination file, using the file hash table
    // lock to help serialize access.
    m_table->lock();

    src_info.m_file->name_write_lock();

    int result = src_info.m_file->try_to_create_destination_file(dest_path);

    src_info.m_file->name_unlock();

    m_table->unlock();
    if (result != 0) { return result; }
    
    // Actually perform the copy.
    {
        int r = this->copy_file_data(src_info);
        if (r!=0) {
            return r;
        }
    }

    // Try to destroy the destination file.
    m_table->lock();

    src_info.m_file->try_to_remove_destination();

    m_table->unlock();

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
static int gettime_reporting_error(struct timespec *ts, backup_callbacks *calls) {
    int r = clock_gettime(CLOCK_MONOTONIC, ts);
    if (r!=0) {
        char string[1000];
        int er = errno;
        if (er!=0) {
            snprintf(string, sizeof(string), "clock_gettime returned an error: errno=%d (%s)", er, strerror(er));
            calls->report_error(er, string);
        } else {
            calls->report_error(-1, "clock_gettime returned an error, but errno==0");
            er = -1;
        }
        return er;
    } else {
        return 0;
    }
}

////////////////////////////////////////////////////////////////////////////////
//
static double tdiff(struct timespec a, struct timespec b)
// Effect: Return a-b in seconds.
{
    return (a.tv_sec - b.tv_sec) + 1e-9*(a.tv_nsec - b.tv_nsec);
}

////////////////////////////////////////////////////////////////////////////////
//
// copy_file_data() -
//
// Description:
//     This section actually copies all the bytes from the source
// file to our newly created backup copy.
//
int copier::copy_file_data(source_info src_info) {
    int r = 0;
    source_file * file = src_info.m_file;
    destination_file * dest = file->get_destination();

    const size_t buf_size = 1024 * 1024;
    char *buf = new char[buf_size]; // this cannot be on the stack.
    ssize_t n_wrote_now = 0;
    size_t poll_string_size = 2000;
    char *poll_string = new char [poll_string_size];
    size_t total_written_this_file = 0;

    struct timespec starttime;
    r = gettime_reporting_error(&starttime, m_calls);
    if (r!=0) goto out;

    while (1) {
        if (!the_manager.copy_is_enabled()) goto out;

        PAUSE(HotBackup::COPIER_BEFORE_READ);
        const ssize_t lock_start = total_written_this_file;
        const ssize_t lock_end   = total_written_this_file + buf_size;
        file->lock_range(lock_start, lock_end);
        ssize_t n_read = call_real_read(src_info.m_fd, buf, buf_size);
        if (n_read == 0) {
            r = file->unlock_range(lock_start, lock_end);
            if (r!=0) goto out;
            break;
        } else if (n_read < 0) {
            r = errno; // Return the error code from the read, not -1.
            int rr __attribute__((unused)) = file->unlock_range(lock_start, lock_end); // Ignore any errors from this, we already have a problem.
            goto out;
        }

        PAUSE(HotBackup::COPIER_AFTER_READ_BEFORE_WRITE);
        ssize_t n_wrote_this_buf = 0;
        while (n_wrote_this_buf < n_read) {
            snprintf(poll_string, 
                     poll_string_size, 
                     "Backup progress %ld bytes, %ld files.  Copying file: %ld/%ld bytes done of %s to %s.",
                     m_total_bytes_backed_up, 
                     m_total_files_backed_up, 
                     total_written_this_file, 
                     src_info.m_size,
                     src_info.m_path,
                     dest->get_path());
            r = m_calls->poll(0, poll_string);
            if (r!=0) {
                m_calls->report_error(r, "User aborted backup");
                int rr __attribute__((unused)) = file->unlock_range(lock_start, lock_end); // ignore any errors from this, we already have a problem
                goto out;
            }

            n_wrote_now = call_real_write(dest->get_fd(),
                                          buf + n_wrote_this_buf,
                                          n_read - n_wrote_this_buf);
            if(n_wrote_now < 0) {
                r = errno;
                snprintf(poll_string, poll_string_size, "error write to %s, errno=%d (%s) at %s:%d", dest->get_path(), r, strerror(r), __FILE__, __LINE__);
                m_calls->report_error(r, poll_string);
                int rr __attribute__((unused)) = file->unlock_range(lock_start, lock_end); // ignore any errors from this since we already have a problem
                goto out;
            }
            n_wrote_this_buf        += n_wrote_now;
            total_written_this_file += n_wrote_now;
            m_total_bytes_backed_up += n_wrote_now;
        }

        r = file->unlock_range(lock_start, lock_end);
        if (r!=0) goto out;

        PAUSE(HotBackup::COPIER_AFTER_WRITE);
        while (1) {
            if (!the_manager.copy_is_enabled()) goto out;

            // Sleep until we've used up enough time.  Be sure to keep polling once per second.
            struct timespec endtime;
            r = gettime_reporting_error(&endtime, m_calls);
            if (r!=0) goto out;
            double actual_time = tdiff(endtime, starttime);
            unsigned long throttle = m_calls->get_throttle();
            double budgeted_time = total_written_this_file / (double)throttle;
            if (budgeted_time <= actual_time) break;
            double sleep_time = budgeted_time - actual_time;  // if we were supposed to copy 10MB at 2MB/s, then our budget was 5s.  If we took 1s, then sleep 4s.
            {
                char string[1000];
                snprintf(string, 
                         sizeof(string),
                         "Backup progress %ld bytes, %ld files.  Throttled: copied %ld/%ld bytes of %s to %s. Sleeping %.2fs for throttling.",
                         m_total_bytes_backed_up,
                         m_total_files_backed_up,
                         total_written_this_file, 
                         src_info.m_size, 
                         src_info.m_path, 
                         dest->get_path(), 
                         sleep_time);
                r = m_calls->poll(0, string);
            }
            if (r!=0) {
                m_calls->report_error(r, "User aborted backup");
                goto out;
            }
            if (sleep_time>1) {
                usleep(1000000);
            } else {
                usleep((long)(sleep_time*1e6));
            }

            if (!the_manager.copy_is_enabled()) goto out;

        }
    }

out:
    delete[] buf;
    delete[] poll_string;
    return r;
}

////////////////////////////////////////////////////////////////////////////////
//
// add_dir_entries_to_todo() - 
//
// Description: 
//
//     Loop through each entry, adding directories and regular
// files to our copy 'todo' list.
//
int copier::add_dir_entries_to_todo(DIR *dir, const char *file)
{
    TRACE("--Adding all entries in this directory to todo list: ", file);
    int error = 0;
    pmutex_lock(&m_todo_mutex);
    struct dirent const *e = NULL;
    while((e = readdir(dir)) != NULL) {
        if (!the_manager.copy_is_enabled()) break;
        if(is_dot(e)) {
            TRACE("skipping: ", e->d_name);
        } else {
            TRACE("-> prepending :", e->d_name);
            TRACE("-> with :", file);
            
            // Concatenate the stripped dir name with this dir entry.
            int length = strlen(file) + strlen(e->d_name) + 2;
            char new_name[length + 1];
            int printed = 0;
            printed = snprintf(new_name, length + 1, "%s/%s", file, e->d_name);
            if(printed + 1 != length) {
                // snprintf had an error.  We must abort the copy.
                error = errno;
                goto out;
            }
            
            // Add it to our todo list.
            m_todo.push_back(strdup(new_name));
            TRACE("~~~Added this file to todo list:", new_name);
        }
    }
    
out:
    pmutex_unlock(&m_todo_mutex);

    return error;
}

////////////////////////////////////////////////////////////////////////////////
//
void copier::add_file_to_todo(const char *file)
{
    pmutex_lock(&m_todo_mutex);
    m_todo.push_back(strdup(file));
    pmutex_unlock(&m_todo_mutex);
}

////////////////////////////////////////////////////////////////////////////////
//
// cleanup() -
//
// Description:
//
//     Frees any strings that are still allocated in our todo list.
//
// Notes:
//
//     This should only be called if there is no future copy work.
//
void copier::cleanup(void) {
    pmutex_lock(&m_todo_mutex);
    for(std::vector<char *>::size_type i = 0; i < m_todo.size(); ++i) {
        char *file = m_todo[i];
        if (file == NULL) {
            continue;
        }

        free((void*)file);
        m_todo[i] = NULL;
    }
    pmutex_unlock(&m_todo_mutex);
}
