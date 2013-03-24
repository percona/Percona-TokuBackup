/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include "backup_copier.h"
#include "real_syscalls.h"
#include "backup_debug.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#if DEBUG_HOTBACKUP
#define WARN(string, arg) HotBackup::CopyWarn(string, arg)
#define TRACE(string, arg) HotBackup::CopyTrace(string, arg)
#define ERROR(string, arg) HotBackup::CopyError(string, arg)
#else
#define WARN(string, arg) 
#define TRACE(string, arg) 
#define ERROR(string, arg) 
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
backup_copier::backup_copier()
: m_source(0), m_dest(0), m_copy_error(0)
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
void backup_copier::set_error(int error)
{
    m_copy_error = error;
}

////////////////////////////////////////////////////////////////////////////////
//
int backup_copier::get_error(void)
{
    return m_copy_error;
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
int backup_copier::do_copy(backup_poll_fun_t poll_fun, void*poll_extra, backup_error_fun_t error_fun, void*error_extra) {
    int r = 0;

    // Start with "."
    m_todo.push_back(strdup("."));
    char *fname = 0;
    size_t n_done  = 0;
    while (size_t n_known = m_todo.size()) {
        
        fname = m_todo.back();
        TRACE("Copying: ", fname);
        char msg[1000];
        snprintf(msg, sizeof(msg), "Copying file number %ld of %ld seen so far (%s)", n_done, n_known, fname);
        msg[sizeof(msg)-1]=0; // be paranoid
        // Use n_done/n_files.   We need to do a better estimate involving n_bytes_copied/n_bytes_total
        // This one is very wrong
        r = poll_fun((double)n_done/(double)(n_done+n_known), msg, poll_extra);
        if (r != 0) goto out;
        m_todo.pop_back();
        r = this->copy_stripped_file(fname, poll_fun, poll_extra, error_fun, error_extra);
        if(r != 0) {
            goto out;
        }
        
        n_done++;
    }

out:
    return r;
}


////////////////////////////////////////////////////////////////////////////////
//
// copy_file() -
//
// Description: 
//
//     Copy's the given file, using this copier object's source and
// destination directory members to determine the exact location
// of the file in both the original and backup locations.
//
int backup_copier::copy_stripped_file(const char *file, backup_poll_fun_t poll_fun, void*poll_extra, backup_error_fun_t error_fun, void*error_extra) {
    int r = 0;
    bool is_dot = (strcmp(file, ".") == 0);
    if (is_dot) {
        // Just copy the root of the backup tree.
        r = this->copy_full_path(m_source, m_dest, "", poll_fun, poll_extra, error_fun, error_extra);
        if (r != 0) {
            goto out;
        }
    } else {
        // Prepend the source directory path to the file name.
        int slen = strlen(m_source) + strlen(file) + 2;
        char full_source_file_path[slen];
        r = snprintf(full_source_file_path, slen, "%s/%s", m_source, file);
        if(r + 1 != slen) {
            goto out;
        }

        // Prepend the destination directory path to the file name.
        int dlen = strlen(m_dest) + strlen(file) + 2;
        char full_dest_file_path[dlen];
        r = snprintf(full_dest_file_path, dlen, "%s/%s", m_dest, file);
        if(r + 1 != dlen) {
            goto out;
        }
        
        r = this->copy_full_path(full_source_file_path, full_dest_file_path, file,
                                 poll_fun, poll_extra, error_fun, error_extra);
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
                                  backup_poll_fun_t poll_fun, void*poll_extra, backup_error_fun_t error_fun, void*error_extra) {
    int r = 0;
    struct stat sbuf;
    r = stat(source, &sbuf);
    // TODO: check return value.
    
    // See if the source path is a directory or a real file.
    if (S_ISREG(sbuf.st_mode)) {
        this->copy_regular_file(source, dest, poll_fun, poll_extra, error_fun, error_extra);
    } else if (S_ISDIR(sbuf.st_mode)) {
        // Make the directory in the backup destination.
        r = call_real_mkdir(dest, 0777);
        if (r < 0) {
            int mkdir_errno = errno;
            if(mkdir_errno != EEXIST) {
                char string[1000];
                snprintf(string, sizeof(string), "error mkdir(\"%s\"), errno=%d (%s)", dest, mkdir_errno, strerror(mkdir_errno));
                error_fun(mkdir_errno, string, error_extra);
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
int backup_copier::copy_regular_file(const char *source, const char *dest, backup_poll_fun_t poll_fun, void*poll_extra, backup_error_fun_t error_fun, void*error_extra)
{
    int r = 0;
    int copy_error = 0;
    int create_error = 0;
    int destfd = 0;
    int srcfd = call_real_open(source, O_RDONLY);
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

    copy_error = copy_file_data(srcfd, destfd, source, dest, poll_fun, poll_extra, error_fun, error_extra);

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


////////////////////////////////////////////////////////////////////////////////
//
// copy_file_data() -
//
// Description:
//     This section actually copies all the bytes from the source
// file to our newly created backup copy.
//
int backup_copier::copy_file_data(int srcfd, int destfd, const char *source_path, const char *dest_path, backup_poll_fun_t poll_fun, void*poll_extra, backup_error_fun_t error_fun, void*error_extra)
{
    int r = 0;

    ssize_t n_read;
    // TODO: Replace these magic numbers.
    const size_t buf_size = 1024 * 1024;
    char *buf = new char[buf_size]; // this cannot be on the stack.
    ssize_t n_wrote_now = 0;
    char poll_string[1000];
    snprintf(poll_string, sizeof(poll_string), "Copying %s to %s", source_path, dest_path);
    while ((n_read = call_real_read(srcfd, buf, buf_size))) {
        if(n_read < 0) {
            r = -1;
            goto out;
        }
        
        ssize_t n_wrote_this_buf = 0;
        while (n_wrote_this_buf < n_read) {
            r = poll_fun(0, poll_string, poll_extra);
            if (r!=0) {
                error_fun(r, "User aborted backup", error_extra);
                goto out;
            }

            n_wrote_now = call_real_write(destfd,
                                          buf + n_wrote_this_buf,
                                          n_read - n_wrote_this_buf);
            if(n_wrote_now < 0) {
                r = errno;
                char string[1000];
                snprintf(string, sizeof(string), "errno=%d (%s) Writing destination file %s", r, strerror(r), dest_path);
                error_fun(r, string, error_extra);
                goto out;
            }

            n_wrote_this_buf += n_wrote_now;
        }
    }

out:
    delete buf;
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
