/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include "backup_copier.h"
#include "real_syscalls.h"
#include "backup_debug.h"
#include "backup_manager.h"
#include "file_hash_table.h"
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

////////////////////////////////////////////////////////////////////////////////
//
// backup_copier() - 
//
// Description:
//
//     Constructor for this copier object.
//
backup_copier::backup_copier(backup_callbacks *calls, file_hash_table * const table)
  : m_source(0), m_dest(0), m_calls(calls), m_table(table)
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
void backup_copier::set_directories(const char *source, const char *dest)
{
    m_source = source;
    m_dest = dest;
    m_copy_error = 0;
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
int backup_copier::do_copy() {
    int r = 0;

    // Start with "."
    m_todo.push_back(strdup("."));
    char *fname = 0;
    uint64_t total_bytes_backed_up = 0;
    uint64_t total_files_backed_up = 0;
    while (size_t n_known = m_todo.size()) {
        
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
        m_todo.pop_back();
        r = this->copy_stripped_file(fname, &total_bytes_backed_up, total_files_backed_up);
        free((void*)fname);
        fname = NULL;
        if(r != 0) {
            goto out;
        }
        total_files_backed_up++;
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
int backup_copier::copy_stripped_file(const char *file, uint64_t *total_bytes_backed_up, const uint64_t total_files_backed_up) {
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
int backup_copier::copy_full_path(const char *source,
                                  const char* dest,
                                  const char *file,
                                  uint64_t *total_bytes_backed_up, const uint64_t total_files_backed_up) {
    int r = 0;
    struct stat sbuf;
    r = stat(source, &sbuf);
    if (r!=0) {
        r = errno;
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
        // Make the directory in the backup destination.
        r = call_real_mkdir(dest, 0777);
        if (r < 0) {
            int mkdir_errno = errno;
            if(mkdir_errno != EEXIST) {
                char *string = malloc_snprintf(strlen(dest)+100, "error mkdir(\"%s\"), errno=%d (%s) at %s:%d", dest, mkdir_errno, strerror(mkdir_errno), __FILE__, __LINE__);
                m_calls->report_error(mkdir_errno, string);
                free(string);
                r = mkdir_errno;
                goto out;
            }
            
            ERROR("Cannot create directory that already exists = ", dest);
        }

        // Open the directory to be copied (source directory, full path).
        DIR *dir = opendir(source);
        if(dir == NULL) {
            r = -1;
            goto out;
        }

        r = this->add_dir_entries_to_todo(dir, file);
        if(r != 0) {
            goto out;
        }

        r = closedir(dir);
        // TODO: return error from closedir...
    } else {
        // TODO: Do we need to add a case for hard links?
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
int backup_copier::copy_regular_file(const char *source, const char *dest, off_t source_file_size, uint64_t *total_bytes_backed_up, const uint64_t total_files_backed_up)
{
    int r = 0;
    int copy_error = 0;
    int create_error = 0;
    int destfd = 0;
    int srcfd = call_real_open(source, O_RDONLY);
    source_file * file = NULL;

    if (srcfd < 0) {
        // TODO: Handle case where file is deleted AFTER backup starts.
        goto out;
    }

    destfd = call_real_open(dest, O_WRONLY | O_CREAT, 0700);
    if (destfd < 0) {
        create_error = errno;
        ERROR("Could not create backup copy of file.", dest);
        if(create_error != EEXIST) {
            r = -1;
            goto close_source_fd;
        }
    }

    // Get a handle on the source file so we can lock ranges as we copy.
    m_table->lock();
    file = m_table->get(source);
    if (file == NULL) {
        TRACE("Creating new source file", source);
        file = new source_file(source);
        int r = file->init();
        if (r != 0) {
	  // TODO: handle rare pthread error.
        }

        m_table->put(file);
    }

    file->add_reference();
    m_table->unlock();
    
    copy_error = copy_file_data(srcfd, destfd, source, dest, file, source_file_size, total_bytes_backed_up, total_files_backed_up);

    m_table->lock();
    m_table->try_to_remove(file);
    m_table->unlock();

    r = call_real_close(destfd);
    if(r != 0) {
        // TODO: What happens when we can't close file descriptors as part of the copy process?
    }
    
close_source_fd:

    r = call_real_close(srcfd);
    if(r != 0) {
        // TODO: What happens when we can't close file descriptors as part of the copy process?
    }

out:
    return copy_error;
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
int backup_copier::copy_file_data(int srcfd, int destfd, const char *source_path, const char *dest_path, source_file * const file, off_t source_file_size, uint64_t *total_bytes_backed_up, const uint64_t total_files_backed_up) {
    int r = 0;

    // TODO: Replace these magic numbers.
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
        PAUSE(HotBackup::COPIER_BEFORE_READ);
        file->lock_range();
        ssize_t n_read = call_real_read(srcfd, buf, buf_size);
        if (n_read == 0) {
            file->unlock_range();
            break;
        } else if (n_read < 0) {
            r = -1;
            file->unlock_range();
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
                file->unlock_range();
                goto out;
            }

            n_wrote_now = call_real_write(destfd,
                                          buf + n_wrote_this_buf,
                                          n_read - n_wrote_this_buf);
            if(n_wrote_now < 0) {
                r = errno;
                snprintf(poll_string, poll_string_size, "error write to %s, errno=%d (%s) at %s:%d", dest_path, r, strerror(r), __FILE__, __LINE__);
                m_calls->report_error(r, poll_string);
                file->unlock_range();
                goto out;
            }
            n_wrote_this_buf        += n_wrote_now;
            total_written_this_file += n_wrote_now;
            *total_bytes_backed_up  += n_wrote_now;
        }

        file->unlock_range();

        PAUSE(HotBackup::COPIER_AFTER_WRITE);
        while (1) {
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
int backup_copier::add_dir_entries_to_todo(DIR *dir, const char *file)
{
    TRACE("--Adding all entries in this directory to todo list: ", file);
    int r = 0;
    struct dirent const *e = NULL;
    while((e = readdir(dir)) != NULL) {
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
                r = -1;
                goto out;
            }
            
            // Add it to our todo list.
            m_todo.push_back(strdup(new_name));
            TRACE("~~~Added this file to todo list:", new_name);
        }
    }
    
out:
    return r;
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
void backup_copier::cleanup(void) {
    for(std::vector<char *>::size_type i = 0; i < m_todo.size(); ++i) {
        char *file = m_todo[i];
        if (file == NULL) {
            continue;
        }

        free((void*)file);
        m_todo[i] = NULL;
    }
}
