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
      m_table(table)
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

    // Start with "."
    m_todo.push_back(strdup("."));
    char *fname = 0;
    uint64_t total_bytes_backed_up = 0;
    uint64_t total_files_backed_up = 0;
    size_t n_known = 0;
    r = pmutex_lock(&m_todo_mutex);
    if (r != 0) goto out;
    n_known = m_todo.size();
    r = pmutex_unlock(&m_todo_mutex);
    if (r != 0) goto out;
    while (n_known != 0) {

        if (!the_manager.copy_is_enabled()) goto out;
        
        fname = m_todo.back();
        TRACE("Copying: ", fname);
        
        char *msg = malloc_snprintf(strlen(fname)+100, "Backup progress %ld bytes, %ld files.  %ld files known of. Copying file %s",  total_bytes_backed_up, total_files_backed_up, n_known, fname);
        // Use n_done/n_files.   We need to do a better estimate involving n_bytes_copied/n_bytes_total
        // This one is very wrongu
        r = m_calls->poll(0, msg);
        free(msg);
        if (r != 0) {
            goto out;
        }

        r = pmutex_lock(&m_todo_mutex);
        if (r != 0) goto out;

        m_todo.pop_back();
        r = pmutex_unlock(&m_todo_mutex);
        if (r != 0) goto out;
        
        r = this->copy_stripped_file(fname, &total_bytes_backed_up, total_files_backed_up);
        free((void*)fname);
        fname = NULL;
        if(r != 0) {
            goto out;
        }
        total_files_backed_up++;
        
        r = pmutex_lock(&m_todo_mutex);
        if (r != 0) goto out;

        n_known = m_todo.size();
        r = pmutex_unlock(&m_todo_mutex);
        if (r != 0) goto out;
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
int copier::copy_stripped_file(const char *file, uint64_t *total_bytes_backed_up, const uint64_t total_files_backed_up) {
    int r = 0;
    bool is_dot = (strcmp(file, ".") == 0);
    if (is_dot) {
        // Just copy the root of the backup tree.
        r = this->copy_full_path(m_source, m_dest, "", total_bytes_backed_up, total_files_backed_up);
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
        
        r = this->copy_full_path(full_source_file_path, full_dest_file_path, file, total_bytes_backed_up, total_files_backed_up);
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
int copier::copy_full_path(const char *source,
                                  const char* dest,
                                  const char *file,
                                  uint64_t *total_bytes_backed_up, const uint64_t total_files_backed_up) {
    int r = 0;
    struct stat sbuf;
    r = stat(source, &sbuf);
    if (r!=0) {
        r = errno;
        // Ignore errors about file not existing, 
        // because we have not yet opened the file with open(), 
        // which would prevent it from disappearing.
        if (r == ENOENT) {
            goto out;
        }

        char *string = malloc_snprintf(strlen(dest)+100, "error stat(\"%s\"), errno=%d (%s) at %s:%d", dest, r, strerror(r), __FILE__, __LINE__);
        m_calls->report_error(errno, string);
        free(string);
        goto out;
    }
    
    // See if the source path is a directory or a real file.
    if (S_ISREG(sbuf.st_mode)) {
        r = this->copy_regular_file(source, dest, sbuf.st_size, total_bytes_backed_up, total_files_backed_up);
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
int copier::copy_regular_file(const char *source, const char *dest, off_t source_file_size, uint64_t *total_bytes_backed_up, const uint64_t total_files_backed_up)
{
    source_file * file = NULL;
    int destfd = 0;
    int srcfd = call_real_open(source, O_RDONLY);
    if (srcfd < 0) {
        int r = errno;
        // Ignore errors about the source file not existing, 
        // because we someone must have deleted the source name
        // since we discovered it and stat'd it.
        if (r == ENOENT) {
            return 0;
        } else {
            the_manager.backup_error(r, "Couldn't open source file %s at %s:%d", source, __FILE__, __LINE__);
            return r;
        }
    }

    destfd = call_real_open(dest, O_WRONLY | O_CREAT, 0700);
    if (destfd < 0) {
        int r = errno;
        if (r == EEXIST) {
            // Some other CAPTURE call has already created the
            // directory, just open it.
            destfd = call_real_open(dest, O_WRONLY);
            if (destfd < 0) {
                r = errno;
                the_manager.backup_error(r, "Couldn't open destination file %s at %s:%d", dest, __FILE__, __LINE__);
                return r;
            }
        } else {
            the_manager.backup_error(r, "Couldn't create destintaion file %s at %s:%d", dest, __FILE__, __LINE__);
            ignore(call_real_close(srcfd)); // ignore any errors here.
            return r;
        }
    }

    // Get a handle on the source file so we can lock ranges as we copy.
    {
        int r = m_table->lock();
        if (r!=0) return r;
    }
    file = m_table->get(source);
    if (file == NULL) {
        TRACE("Creating new source file", source);
        file = new source_file();
        int r = file->init(source);
        if (r != 0) {
            // Already reported the error, so ignore these errors.
            ignore(m_table->unlock());
            ignore(call_real_close(destfd));
            ignore(call_real_close(srcfd));
            return r;
        }
        m_table->put(file);
    }

    file->add_reference();
    {
        int r = m_table->unlock();
        if (r!=0) return r;
    }
    
    {
        int r = copy_file_data(srcfd, destfd, source, dest, file, source_file_size, total_bytes_backed_up, total_files_backed_up);
        if (r!=0) {
            // Already reported the error, so ignore these errors.
            ignore(call_real_close(destfd));
            ignore(call_real_close(srcfd));
            return r;
        }
    }

    {
        int r = m_table->lock();
        if (r!=0) return r;
    }
    m_table->try_to_remove(file);
    {
        int r = m_table->unlock();
        if (r!=0) return r;
    }

    {
        int r = call_real_close(destfd);
        if (r != 0) {
            r = errno;
            the_manager.backup_error(r, "Could not close %s at %s:%d", dest, __FILE__, __LINE__);
            ignore(call_real_close(srcfd));
            return r;
        }
    }
    
    {
        int r = call_real_close(srcfd);
        if (r != 0) {
            r = errno;
            the_manager.backup_error(r, "Could not close %s at %s:%d", source, __FILE__, __LINE__);
            return r;
        }
    }

    return 0;
}


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
int copier::copy_file_data(int srcfd, int destfd, const char *source_path, const char *dest_path, source_file * const file, off_t source_file_size, uint64_t *total_bytes_backed_up, const uint64_t total_files_backed_up) {
    int r = 0;

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
        r = file->lock_range(lock_start, lock_end);
        if (r!=0) {
            goto out;
        }
        ssize_t n_read = call_real_read(srcfd, buf, buf_size);
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
            snprintf(poll_string, poll_string_size, "Backup progress %ld bytes, %ld files.  Copying file: %ld/%ld bytes done of %s to %s.",
                     *total_bytes_backed_up, total_files_backed_up, total_written_this_file, source_file_size, source_path, dest_path);
            r = m_calls->poll(0, poll_string);
            if (r!=0) {
                m_calls->report_error(r, "User aborted backup");
                int rr __attribute__((unused)) = file->unlock_range(lock_start, lock_end); // ignore any errors from this, we already have a problem
                goto out;
            }

            n_wrote_now = call_real_write(destfd,
                                          buf + n_wrote_this_buf,
                                          n_read - n_wrote_this_buf);
            if(n_wrote_now < 0) {
                r = errno;
                snprintf(poll_string, poll_string_size, "error write to %s, errno=%d (%s) at %s:%d", dest_path, r, strerror(r), __FILE__, __LINE__);
                m_calls->report_error(r, poll_string);
                int rr __attribute__((unused)) = file->unlock_range(lock_start, lock_end); // ignore any errors from this since we already have a problem
                goto out;
            }
            n_wrote_this_buf        += n_wrote_now;
            total_written_this_file += n_wrote_now;
            *total_bytes_backed_up  += n_wrote_now;
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
                snprintf(string, sizeof(string), "Backup progress %ld bytes, %ld files.  Throttled: copied %ld/%ld bytes of %s to %s. Sleeping %.2fs for throttling.",
                         *total_bytes_backed_up, total_files_backed_up,
                         total_written_this_file, source_file_size, source_path, dest_path, sleep_time);
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
    {
        int r = pmutex_lock(&m_todo_mutex);
        if (r != 0) return r;
    }
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
    {
        int r = pmutex_unlock(&m_todo_mutex);
        if (r != 0) return r;
    }

    return error;
}

////////////////////////////////////////////////////////////////////////////////
//
void copier::add_file_to_todo(const char *file)
{
    int r = pmutex_lock(&m_todo_mutex);
    if (r != 0) {
        return;
    }
    m_todo.push_back(strdup(file));
    r = pmutex_unlock(&m_todo_mutex);
    if (r != 0) {
        return;
    }
    // TODO. Handle those errors properly.
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
    for(std::vector<char *>::size_type i = 0; i < m_todo.size(); ++i) {
        char *file = m_todo[i];
        if (file == NULL) {
            continue;
        }

        free((void*)file);
        m_todo[i] = NULL;
    }
}
