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
#include "check.h"
#include "copier.h"
#include "file_hash_table.h"
#include "manager.h"
#include "mutex.h"
#include "raii-malloc.h"
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

#if defined(DEBUG_HOTBACKUP) && DEBUG_HOTBACKUP
#define WARN(string, arg) HotBackup::CopyWarn(string, arg)
#define TRACE(string, arg) HotBackup::CopyTrace(string, arg)
#define ERROR(string, arg) HotBackup::CopyError(string, arg)
#else
#define WARN(string, arg) 
#define TRACE(string, arg) 
#define ERROR(string, arg) 
#endif

#if defined(PAUSE_POINTS_ON)
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
copier::copier(backup_callbacks *calls, file_hash_table * const table) throw()
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
void copier::set_directories(const char *source, const char *dest) throw() {
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
int copier::do_copy(void) throw() {
    m_total_bytes_to_back_up = dirsum(m_source);
    int r = 0;
    char *fname = 0;
    size_t n_known = 0;
    {
        with_mutex_locked tm(&m_todo_mutex, BACKTRACE(NULL));
        // Start with "."
        m_todo.push_back(strdup("."));
        n_known = m_todo.size();
    }
    while (n_known != 0) {

        if (!the_manager.copy_is_enabled()) goto out;
        
        {
            with_mutex_locked tm(&m_todo_mutex, BACKTRACE(NULL));
            fname = m_todo.back();
        }
        TRACE("Copying: ", fname);
        
        char *msg = malloc_snprintf(strlen(fname)+100, "Backup progress %ld bytes, %ld files.  %ld more files known of. Copying file %s",  m_total_bytes_backed_up, m_total_files_backed_up, n_known, fname);
        // Use n_done/n_files.   We need to do a better estimate involving n_bytes_copied/n_bytes_total
        // This one is very wrongu
        r = m_calls->poll((double)(m_total_bytes_backed_up+1)/(double)(m_total_bytes_to_back_up+1), msg);
        free(msg);
        if (r != 0) {
            fprintf(stderr, "%s:%d poll error r=%d\n", __FILE__, __LINE__, r);
            goto out;
        }

        {
            with_mutex_locked tm(&m_todo_mutex, BACKTRACE(NULL));
            m_todo.pop_back();
        }
        
        r = this->copy_stripped_file(fname);
        if(r != 0) {
            fprintf(stderr, "%s:%d copy error fname=%s r=%d\n", __FILE__, __LINE__, fname, r);
            free((void*)fname);
            fname = NULL;
            goto out;
        }
        free((void*)fname);
        fname = NULL;

        m_total_files_backed_up++;
        
        {
            with_mutex_locked tm(&m_todo_mutex, BACKTRACE(NULL));
            n_known = m_todo.size();
        }
    }

out:
    this->cleanup();
    return r;
}


static void pathcat(char *dest, size_t destlen, const char *a, int alen, const char *b) throw()
// Effect: Concatenate paths A and B (insert a / between if needed) into dest.  If a ends with a / and b starts with a / then put only 1 / in.
// Requires: destlen is big enough.  A is nonempty.
{
    bool a_has_slash_at_end = (alen>0) && (a[alen-1]=='/');
    if (b[0]=='/') b++;
    int r = snprintf(dest, destlen, "%s%s%s", a, a_has_slash_at_end ? "" : "/", b);
    check(r<(int)destlen);
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
int copier::copy_stripped_file(const char *file) throw() {
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
int copier::copy_full_path(const char *source, const char* dest, const char *file) throw() {
    if (m_calls->exclude_copy(source))
        return 0;
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
        char *string = malloc_snprintf(strlen(dest)+100, "Could not stat(\"%s\"), errno=%d (%s) at %s:%d", dest, r, strerror(r), __FILE__, __LINE__);
        m_calls->report_error(errno, string);
        free(string);
        goto out;
    }
    
    // See if the source path is a directory or a real file.
    if (S_ISREG(sbuf.st_mode)) {
        source_info src_info = {-1, source, sbuf.st_size, NULL, O_RDONLY};
        r = this->copy_using_source_info(src_info, dest);
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
                the_manager.backup_error(r, "Could not opendir %s", source);
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
int copier::copy_regular_file(source_info src_info, const char *dest) throw() {
    src_info.m_flags = O_RDONLY;
    if(src_info.m_file->locked_direct_io_flag_is_set()) {
        src_info.m_flags |= O_DIRECT;
    }

    src_info.m_fd = call_real_open(src_info.m_path, src_info.m_flags);
    if (src_info.m_fd < 0) {
        int error = errno;
        if (error == ENOENT) {
            return 0;
        } else {
            the_manager.backup_error(error, "Could not open source file: %s", src_info.m_path);
            return error;
        }
    }

    PAUSE(HotBackup::COPIER_AFTER_OPEN_SOURCE);

    //source_info src_info = {srcfd, source, source_file_size, NULL};
    //int result = this->copy_using_source_info(src_info, dest);
    int result = this->create_destination_and_copy(src_info, dest);
    
    int r = call_real_close(src_info.m_fd);
    if (r != 0) {
        r = errno;
        the_manager.backup_error(r, "Could not close %s at %s:%d", src_info.m_path, __FILE__, __LINE__);
        return r;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////
//
int copier::copy_using_source_info(source_info src_info, const char *path) throw() {
    source_file * file = NULL;
    TRACE("Creating new source file", path);
    m_table->get_or_create_locked(src_info.m_path, &file);

    src_info.m_file = file;
    int result = this->copy_regular_file(src_info, path);

    m_table->try_to_remove_locked(file);
    return result;
}

////////////////////////////////////////////////////////////////////////////////
//
int copier::create_destination_and_copy(source_info src_info,  const char *path) throw() {
    TRACE("Creating new destination file", path);
    with_object_to_free<char*> dest_path(strdup(path));
    if (dest_path.value == NULL) {
        int r = errno;
        return r;
    }

    // Try to create the destination file, using the file hash table
    // lock to help serialize access.
    bool source_exists = true;
    int result = 0;
    {
        with_file_hash_table_mutex mtl(m_table);

        with_source_file_name_write_lock sfl(src_info.m_file);

        // Check to see if the real source file still exists.  If it
        // doesn't, it has been unlinked and we should NOT create the
        // destination file.  We are protected by the table lock here.
        struct stat buf;
        TRACE("stat'ing file = ", src_info.m_path);
        int stat_r = lstat(src_info.m_path, &buf);
        if (stat_r == 0) {
            result = src_info.m_file->try_to_create_destination_file(dest_path.value);
        } else {
            source_exists = false;
        }

        // If the source file was unlinked since the respective
        // source_file object was created and since the stat
        // succeeded, we should not proceed.
        if (src_info.m_file->get_destination() == NULL) {
            source_exists = false;
        }
    }

    if (result != 0) { return result; }

    if (source_exists) {
        // Actually perform the copy.
        int r = this->copy_file_data(src_info);
        if (r!=0) {
            return r;
        }
    }

    // Try to destroy the destination file.
    {
        with_file_hash_table_mutex mtl(m_table);

        src_info.m_file->try_to_remove_destination();
    }

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
static int gettime_reporting_error(struct timespec *ts, backup_callbacks *calls) throw() {
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
static double tdiff(struct timespec a, struct timespec b) throw()
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
int copier::copy_file_data(source_info src_info) throw() {
    int r = 0;
    // For DirectIO: we need to allocate a mem-aligned buffer.
    const size_t align = 2<<12; // why 8K?
    const size_t buf_size = 1024 * 1024;
    char *buf_base = new char[buf_size + align];
    char *buf = (char *)(((size_t)buf_base + align) & ~(align-1));

    source_file * file = src_info.m_file;
    destination_file * dest = file->get_destination();
    TRACE("Copying to file:", dest->get_path());
    // Polling variables.
    ssize_t n_wrote_now = 0;
    size_t poll_string_size = 2000;
    char *poll_string = new char [poll_string_size];
    m_total_written_this_file = 0;
    struct timespec starttime;

    r = gettime_reporting_error(&starttime, m_calls);
    if (r!=0) goto out;

    while (1) {
        if (!the_manager.copy_is_enabled()) goto out;

        PAUSE(HotBackup::COPIER_BEFORE_READ);
        const ssize_t lock_start = m_total_written_this_file;
        const ssize_t lock_end   = m_total_written_this_file + buf_size;
        file->lock_range(lock_start, lock_end);
        
        copy_result result;
        result = open_and_lock_file_then_copy_range(src_info, buf, buf_size, poll_string, poll_string_size);
        n_wrote_now = result.m_n_wrote_now;

        r = file->unlock_range(lock_start, lock_end); 
        if (r!=0) goto out;

        // If we hit an error, or have no more bytes to write, 
        // we are finished and need to return immediately.
        if (result.m_result != 0 || n_wrote_now == 0)
        {
            r = result.m_result;
            goto out;
        }

        PAUSE(HotBackup::COPIER_AFTER_WRITE);
        r = possibly_sleep_or_abort(src_info, m_total_written_this_file, dest, starttime);
        if (r != 0) {
            goto out;
        }
    }

out:
    delete[] buf_base;
    delete[] poll_string;
    return r;
}

////////////////////////////////////////////////////////////////////////////////
//
copy_result copier::open_and_lock_file_then_copy_range(source_info src_info, 
                                                       char *buf, 
                                                       size_t buf_size, 
                                                       char *poll_string, 
                                                       size_t poll_string_size) throw()
{
    copy_result result;
    with_source_file_fd_lock fdl(src_info.m_file);
    // We may have to re-open the source file because the Direct I/O
    // flags may have changed since we last copied a range.
    if (src_info.m_file->given_flags_are_different(src_info.m_flags)) {
        // Close the old fd.
        int r = call_real_close(src_info.m_fd);
        if (r != 0) {
            int close_errno = errno;
            the_manager.backup_error(close_errno, "Could not close %s at %s:%d", src_info.m_path, __FILE__, __LINE__);
            result.m_result = close_errno;
        }

        // Open the new fd.
        int flags = O_RDONLY;
        if (src_info.m_file->direct_io_flag_is_set()) {
            flags |= O_DIRECT;
        }

        src_info.m_fd = call_real_open(src_info.m_path, flags);
        if (src_info.m_fd < 0) {
            int open_errno = errno;
            if (open_errno == ENOENT) {
                return result;
            } else {
                the_manager.backup_error(open_errno, "Could not open source file: %s", src_info.m_path);
                result.m_result = open_errno;
                return result;
            }
        }

        // We have to do a seek because we close and re-open the file
        // between each range copy.  For host files opened with the
        // O_DIRECT flag, m_total_written_this_file should line up
        // with the correct offsets and should not return an error.
        off_t offset = call_real_lseek(src_info.m_fd, m_total_written_this_file, SEEK_SET);
        if (offset < 0) {
            int lseek_errno = errno;
            the_manager.backup_error(lseek_errno, "Could not lseek file: %s", src_info.m_path);
            result.m_result = lseek_errno;
            return result;
        }
    }

    result = copy_file_range(src_info,
                             buf, 
                             buf_size, 
                             poll_string, 
                             poll_string_size);

    return result;
}


////////////////////////////////////////////////////////////////////////////////
//
copy_result copier::copy_file_range(source_info src_info,
                                    char * buf, 
                                    size_t buf_size, 
                                    char *poll_string, 
                                    size_t poll_string_size) throw()
{
    copy_result result;
    result.m_result = 0;
    result.m_n_wrote_now = 0;
    destination_file * dest = src_info.m_file->get_destination();
        ssize_t n_read = call_real_read(src_info.m_fd, buf, buf_size);
        if (n_read == 0) {
            // SUCCESS! We are done copying the file.
            result.m_result = 0;
            result.m_n_wrote_now = 0;
            return result;
        } else if (n_read < 0) {
            int read_errno = errno;
            snprintf(poll_string, poll_string_size, "Could not read from %s, n_read=%lu errno=%d (%s) fd=%d at %s:%d", src_info.m_path, n_read, read_errno, strerror(read_errno), src_info.m_fd, __FILE__, __LINE__);
            m_calls->report_error(read_errno, poll_string);
            result.m_result = read_errno;
            return result;
        }

        PAUSE(HotBackup::COPIER_AFTER_READ_BEFORE_WRITE);
        ssize_t n_wrote_this_buf = 0;
        while (n_wrote_this_buf < n_read) {
            snprintf(poll_string, 
                     poll_string_size, 
                     "Backup progress %ld bytes, %ld files.  Copying file: %ld/%ld bytes done of %s to %s.",
                     m_total_bytes_backed_up, 
                     m_total_files_backed_up, 
                     m_total_written_this_file, 
                     src_info.m_size,
                     src_info.m_path,
                     dest->get_path());
            int r = m_calls->poll((double)(m_total_bytes_backed_up+1)/(double)(m_total_bytes_to_back_up+1), poll_string);
            if (r!=0) {
                m_calls->report_error(r, "User aborted backup");
                result.m_result = r;
                return result;
            }

            result.m_n_wrote_now = call_real_write(dest->get_fd(),
                                                   buf + n_wrote_this_buf,
                                                   n_read - n_wrote_this_buf);
            if(result.m_n_wrote_now < 0) {
                int write_errno = errno;
                snprintf(poll_string, poll_string_size, "error write to %s, errno=%d (%s) at %s:%d", dest->get_path(), write_errno, strerror(write_errno), __FILE__, __LINE__);
                m_calls->report_error(write_errno, poll_string);
                result.m_result = write_errno;
                return result;
            }

            n_wrote_this_buf          += result.m_n_wrote_now;
            m_total_written_this_file += result.m_n_wrote_now;
            m_total_bytes_backed_up   += result.m_n_wrote_now;
        }

    return result;
}


int copier::possibly_sleep_or_abort(source_info src_info, ssize_t total_written_this_file, destination_file * dest, struct timespec starttime) throw()
{
    int r = 0;
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
                r = m_calls->poll((double)(m_total_bytes_backed_up+1)/(double)(m_total_bytes_to_back_up+1), string);
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
out:
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
int copier::add_dir_entries_to_todo(DIR *dir, const char *file) throw() {
    TRACE("--Adding all entries in this directory to todo list: ", file);
    int error = 0;
    with_mutex_locked tm(&m_todo_mutex, BACKTRACE(NULL));
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
    return error;
}

////////////////////////////////////////////////////////////////////////////////
//
void copier::add_file_to_todo(const char *file) throw() {
    with_mutex_locked tm(&m_todo_mutex, BACKTRACE(NULL));
    m_todo.push_back(strdup(file));
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
void copier::cleanup(void) throw() {
    with_mutex_locked tm(&m_todo_mutex, BACKTRACE(NULL));
    for(std::vector<char *>::size_type i = 0; i < m_todo.size(); ++i) {
        char *file = m_todo[i];
        if (file == NULL) {
            continue;
        }

        free((void*)file);
        m_todo[i] = NULL;
    }
}

bool copier::file_should_be_excluded(const char *file) throw() {
    if (m_calls->exclude_copy(file)) {
        return true;
    }

    return false;
}
