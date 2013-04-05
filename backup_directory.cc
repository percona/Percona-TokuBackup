/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include "backup_directory.h"
#include "file_description.h"
#include "backup_debug.h"
#include "real_syscalls.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>


//////////////////////////////////////////////////////////////////////////////
//
static void print_time(const char *toku_string) {
    time_t t;
    char buf[27];
    time(&t);
    ctime_r(&t, buf);
    fprintf(stderr, "%s %s\n", toku_string, buf);
}


//////////////////////////////////////////////////////////////////////////////
//
//backup_callbacks::backup_callbacks(backup_poll_fun_t poll_fun, 
//                                   void *poll_extra, 
//                                   backup_error_fun_t error_fun, 
//                                   void *error_extra,
//                                   backup_throttle_fun_t throttle_fun)
//: m_poll_function(poll_fun), 
//m_poll_extra(poll_extra), 
//m_error_function(error_fun), 
//m_error_extra(error_extra),
//m_throttle_function(throttle_fun)
//{}
//
////////////////////////////////////////////////////////////////////////////////
////
//int backup_callbacks::poll(float progress, const char *progress_string)
//{
//    int r = 0;
//    r = m_poll_function(progress, progress_string, m_poll_extra);
//    return r;
//}
//
////////////////////////////////////////////////////////////////////////////////
////
//void backup_callbacks::report_error(int error_number, const char *error_str)
//{
//    m_error_function(error_number, error_str, m_error_extra);
//}
//
////////////////////////////////////////////////////////////////////////////////
////
//unsigned long backup_callbacks::get_throttle(void)
//{
//    return m_throttle_function();
//}

//////////////////////////////////////////////////////////////////////////////
//
backup_session::backup_session(const char* source, const char *dest, backup_callbacks *calls, int *errnum)
: m_source_dir(NULL), m_dest_dir(NULL), m_copier(calls)
{
    // TODO: assert that the directory's are not the same.
    // TODO: assert that the destination directory is empty.

    // This code is ugly because we are using a constructor.  We need to do the error propagation now, while we have the source and dest paths,
    // instead of later in what used to be the set_directories() method.  BTW, the google style guide prohibits using constructors.

    int r = 0;
    m_source_dir = realpath(source, NULL);
    m_dest_dir   = realpath(dest,   NULL);
    if (!m_dest_dir) {
        char *str = malloc_snprintf(strlen(dest) + 100, "This backup destination directory does not exist: %s", dest);
        calls->report_error(ENOENT, str); 
        r = ENOENT;
        free(str);
    }
    if (!m_source_dir) {
        char *str = malloc_snprintf(strlen(source) + 100, "This backup source directory does not exist: %s", source);
        calls->report_error(ENOENT, str);
        r = ENOENT;
        free(str);
    }
    m_copier.set_directories(m_source_dir, m_dest_dir);
    *errnum = r;
}


//////////////////////////////////////////////////////////////////////////////
//
backup_session::~backup_session()
{
    if(m_source_dir) {
        free((void*)m_source_dir);
    }

    if(m_dest_dir) {
        free((void*)m_dest_dir);
    }
    
    // TODO: Cleanup copier?
}

//////////////////////////////////////////////////////////////////////////////
//
int backup_session::do_copy()
{
    print_time("Toku Hot Backup: Started:");    
    int r = m_copier.do_copy();
    print_time("Toku Hot Backup: Finished:");
    return r;
}

///////////////////////////////////////////////////////////////////////////////
//
// is_prefix():
//
// Description: 
//
//     Determines if the given file name is within our source
// directory or not.
//
bool backup_session::is_prefix(const char *file)
{
    // mallocing this to make memcheck happy.  I don't like the extra malloc, but I'm more worried about testability than speed right now. -Bradley
    char *absfile = realpath(file, NULL);
    for (int i = 0; true; i++) {
        if (m_source_dir[i] == 0) {
            free(absfile);
            return true;
        }
        if (m_source_dir[i] != absfile[i]) {
            free(absfile);
            return false;
        }
    }
}

static int does_file_exist(const char*);

///////////////////////////////////////////////////////////////////////////////
//
// open_path():
//
// Description:
//
//     Creates backup path for given file if it doesn't exist already.
//
int open_path(const char *file_path) {
    int r = 0;
    // See if the file exists in the backup copy already...
    int exists = does_file_exist(file_path);
    
    if (exists == 0) {
        r = create_subdirectories(file_path);
    }
    
    if(exists == -1) {
        r = -1;
    }
    
    return r;
}


///////////////////////////////////////////////////////////////////////////////
//
// create_subdirectories:
//
// Description:
//
//     Recursively creates all the backup subdirectories 
// required for the given path.
//
int create_subdirectories(const char *path) {
    const char SLASH = '/';
    char *directory = strdup(path);
    char *next_slash = directory;
    ++next_slash;

    int r = 0;

    while(next_slash != NULL) {
        // 1. scan for next slash
        while(*next_slash != 0) {
            if(*next_slash == SLASH) {
                break;
            }
            
            ++next_slash;
        }
        
        // 2. if found /0, set to null, break.
        if (*next_slash == 0) {
            next_slash = NULL;
            break;
        }
        
        // 3. turn slash into NULL
        *next_slash = 0;
        
        // 4. mkdir
        if (*directory) {
            r = call_real_mkdir(directory, 0777);
        
            if(r) {
                //printf("WARN: <CAPTURE>: %s\n", directory);
                // For now, just ignore already existing dir,
                // this is a race between the backup copier
                // and the intercepted open() calls.
                if(errno != EEXIST) {
                    r = errno;
                    goto out;
                } else {
                    // EEXIST is ok
                    r = 0;
                }
            }
        }
        
        // 5. revert slash back to slash char.
        *next_slash = SLASH;
        
        // 6. Advance slash position by 1, to search 
        // past recently found slash, and repeat.
        ++next_slash;
    }
    
out:
    free((void*)directory);
    return r;
}


//////////////////////////////////////////////////////////////////////////////
//
// translate_prefix():
//
// Description: 
//
char* backup_session::translate_prefix(const char *file)
{
    char *absfile = realpath(file, NULL);

    // TODO: Should we have a copy of these lengths already?
    size_t len_op = strlen(m_source_dir);
    size_t len_np = strlen(m_dest_dir);
    size_t len_s = strlen(absfile);
    size_t new_len = len_s - len_op + len_np;
    char *new_string = NULL;
    new_string = (char *)calloc(new_len + 1, sizeof(char));
    memcpy(new_string, m_dest_dir, len_np);
    
    // Copy the file name from the directory with the newline at the end.
    memcpy(new_string + len_np, absfile + len_op, len_s - len_op + 1);
    free(absfile);
    return new_string;
}


//////////////////////////////////////////////////////////////////////////////
//
// does_file_exist():
//
// Description:
//
static int does_file_exist(const char *file) {
    int result = 0;
    struct stat sb;
    // We use stat to determine if the file does not exist.
    int r = stat(file, &sb);
    if (r < 0) {
        int error = errno;
        // We want to catch all other errors.
        if (error != ENOENT) {
            perror("Toku Hot Backup:stat() failed, no file information.");
            result = -1;
        }
    } else {
        result = 1;
    }

    return result;
}

///////////////////////////////////////////////////////////////////////////////
//
char * backup_session::capture_open(const char *file)
{
    char *backup_file_name = NULL;
    if(!this->is_prefix(file)) {
        return NULL;
    }
    
    backup_file_name = this->translate_prefix(file);
    int r = open_path(backup_file_name);
    if (r != 0) {
        // TODO: open path error, abort backup.
        this->abort();
        free((void*)backup_file_name);
        backup_file_name = NULL;
    }
    
    return backup_file_name;
}

///////////////////////////////////////////////////////////////////////////////
//
char * backup_session::capture_create(const char *file)
{
    return this->capture_open(file);
}

///////////////////////////////////////////////////////////////////////////////
//
int backup_session::capture_mkdir(const char *pathname)
{
    if(!this->is_prefix(pathname)) {
        return 0;
    }

    char *backup_directory_name = this->translate_prefix(pathname);
    int r = open_path(backup_directory_name);
    free((void*)backup_directory_name);
    return r;
}

///////////////////////////////////////////////////////////////////////////////
//
// set_directories():
//
// Description: 
// Note: source and dest are copied (so the caller may free them immediately or otherwise reuse the strings).
//
//int backup_directory::set_directories(const char *source, const char *dest,
//                                      backup_poll_fun_t poll_fun __attribute__((unused)), void *poll_extra __attribute__((unused)),
//                                      backup_error_fun_t error_fun, void *error_extra)
//{
//    m_source_dir = realpath(source, NULL);
//    m_dest_dir =   realpath(dest,   NULL);
//    if (m_source_dir==NULL) {
//        char string[1000];
//        snprintf(string, sizeof(string), "Source directory %s does not exist", source);
//        error_fun(ENOENT, string, error_extra);
//        return ENOENT;
//    }
//    if (m_dest_dir==NULL) {
//        free((void*)m_source_dir);
//        m_source_dir = NULL;
//        char string[1000];
//        snprintf(string, sizeof(string), "Destination directory %s does not exist", dest);
//        error_fun(ENOENT, string, error_extra);
//        return ENOENT;
//    }
//    return 0;
//}

