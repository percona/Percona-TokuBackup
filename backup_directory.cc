/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

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

///////////////////////////////////////////////////////////////////////////////
//
// start_copying():
//
// Description: 
//
void *start_copying(void *);
void *start_copying(void * copier) {
    void *r = 0;
    backup_copier *c = (backup_copier*)copier;
    print_time("Toku Hot Backup: Started:");
    int copy_error = c->start_copy();

    if (copy_error != 0) {
        c->set_error(copy_error);
    }

    // TODO: Free todo list strings.
    print_time("Toku Hot Backup: Finished:");
    return r;
}


///////////////////////////////////////////////////////////////////////////////
//
// backup_directory():
//
// Description: 
//
backup_directory::backup_directory()
: m_source_dir(NULL), m_dest_dir(NULL)
{
}


///////////////////////////////////////////////////////////////////////////////
//
// directories_set():
//
// Description: 
//
//     Determines if the source and destination directories have been
// set for this backup_directory object.
//
bool backup_directory::directories_set()
{
    if (m_dest_dir && m_source_dir) {
        return true;
    }

    return false;
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
bool backup_directory::is_prefix(const char *file)
{
    char absfile[PATH_MAX+1];
    realpath(file, absfile);
    for (int i = 0; true; i++) {
        if (m_source_dir[i] == 0) return true;
        if (m_source_dir[i] != absfile[i]) return false;
    }
}


///////////////////////////////////////////////////////////////////////////////
//
// open_path():
//
// Description:
//
//     Creates backup path for given file if it doesn't exist already.
//
int backup_directory::open_path(const char *file_path)
{    
    int r = 0;
    // See if the file exists in the backup copy already...
    int exists = this->does_file_exist(file_path);
    
    if (exists == 0) {
        this->create_subdirectories(file_path);
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
void backup_directory::create_subdirectories(const char *path)
{
    const char SLASH = '/';
    
    // TODO: Is this a memory leak?
    char *directory = strdup(path);
    char *next_slash = directory;
    ++next_slash;

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
        int r = 0;
        if (*directory) {
            r = call_real_mkdir(directory, 0777);
        }
        
        if(r) {
            int error = errno;
            
            //printf("WARN: <CAPTURE>: %s\n", directory);
            // For now, just ignore already existing dir,
            // this is a race between the backup copier
            // and the intercepted open() calls.
            if(error != EEXIST) {
                // TODO: Handle this screw case.
                perror("Toku Hot Backup: Making backup subdirectory failed:");
            }
        }
        
        // 5. revert slash back to slash char.
        *next_slash = SLASH;
        
        // 6. Advance slash position by 1, to search 
        // past recently found slash, and repeat.
        ++next_slash;
    }
}


//////////////////////////////////////////////////////////////////////////////
//
// translate_prefix():
//
// Description: 
//
char* backup_directory::translate_prefix(const char *file)
{
    char absfile[PATH_MAX+1];
    realpath(file, absfile);

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
    return new_string;
}


//////////////////////////////////////////////////////////////////////////////
//
// does_file_exist():
//
// Description:
//
int backup_directory::does_file_exist(const char *file)
{
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
// set_directories():
//
// Description: 
// Note: source and dest are copied (so the caller may free them immediately or otherwise reuse the strings).
//
void backup_directory::set_directories(const char *source, const char *dest)
{
    m_source_dir = realpath(source, NULL);
    m_dest_dir =   realpath(dest,   NULL);
}

//////////////////////////////////////////////////////////////////////////////
//
int backup_directory::set_source_directory(const char *source)
{
    int r = 0;
    // NOTE: This allocates new memory for the fully resolved path.
    m_source_dir = realpath(source, NULL);

    // TODO: Return an error if realpath returns an error.
    return r;
}

//////////////////////////////////////////////////////////////////////////////
//
int backup_directory::set_destination_directory(const char *destination)
{
    int r = 0;
    // NOTE: This allocates new memory for the fully resolved path.
    m_dest_dir = realpath(destination, NULL);
    // TODO: Return an error if realpath returns an error.
    return r;
}

//////////////////////////////////////////////////////////////////////////////
//
// start_copy():
//
// Description: 
//
//     Copies all files and subdirectories to the destination.
//
int backup_directory::start_copy(void)
{
    int r = 0;
    m_copier.set_directories(m_source_dir, m_dest_dir);
    r = pthread_create(&m_thread, NULL, &start_copying, (void*)&m_copier);
    if (r != 0) {
        perror("Toku Hot Backup: Cannot create backup copy thread.");
    }

    return r;
}


//////////////////////////////////////////////////////////////////////////////
//
// wait_for_copy_to_finish():
//
// Description:
//
void backup_directory::wait_for_copy_to_finish(void)
{
    int r = pthread_join(m_thread, NULL);
    if (r) {
        perror("Toku Hot Backup: pthread error: ");
    }
}

//////////////////////////////////////////////////////////////////////////////
//
void backup_directory::abort_copy(void)
{
    // Stop copy thread.
    pthread_cancel(m_thread);
    int r = pthread_join(m_thread, NULL);
    if (r) {
        perror("Toku Hot Backup: pthread error: ");
    }

    // TODO: Clean up copier.        
}

//////////////////////////////////////////////////////////////////////////////
//
int backup_directory::get_error_status(void)
{
    return m_copier.get_error();
}

