/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#include "backup_directory.h"
#include "file_description.h"
#include "backup_debug.h"
#include "real_syscalls.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <sys/stat.h>


///////////////////////////////////////////////////////////////////////////////
//
// start_copying():
//
// Description: 
//
void *start_copying(void *);
void *start_copying(void * copier)
{
    void *r = 0;
    if (DEBUG_HOTBACKUP) printf(">>> pthread: copy started\n");
    backup_copier *c = (backup_copier*)copier;
    c->start_copy();
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
void backup_directory::open_path(const char *file_path)
{    
    // See if the file exists in the backup copy already...
    bool exists = this->does_file_exist(file_path);
    
    if (!exists) {
        this->create_subdirectories(file_path);
    }
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
    char *directory = strdup(path);
    char *next_slash = directory;
    
    assert(*next_slash=='/'); // it's an absolute path.  / always exists.
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
            //perror("WARN: <CAPTURE>: Making subdirectory failed:");
            //printf("WARN: <CAPTURE>: %s\n", directory);
            // For now, just ignore already existing dir,
            // this is a race between the backup copier
            // and the intercepted open() calls.
            assert(error == EEXIST);
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
    
    // TODO: What is this assert checking?
    //assert(len_op < len_s);
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
bool backup_directory::does_file_exist(const char *file)
{
    bool result = false;
    struct stat sb;
    // We use stat to determine if the file does not exist.
    int r = stat(file, &sb);
    if (r < 0) {
        int error = errno;
        // We want to catch all other errors.
        if (error != ENOENT) {
            perror("stat() failed, no backup file information.");
            abort();
        }
    } else {
        result = true;
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
    assert(source);
    assert(dest);
    m_source_dir = realpath(source, NULL);
    m_dest_dir =   realpath(dest,   NULL);
}

int backup_directory::set_source_directory(const char *source)
{
    int r = 0;
    // NOTE: This allocates new memory for the fully resolved path.
    m_source_dir = realpath(source, NULL);

    // TODO: Return an error if realpath returns an error.
    return r;
}

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
void backup_directory::start_copy()
{
    int r = 0;
    m_copier.set_directories(m_source_dir, m_dest_dir);
    r = pthread_create(&m_thread, NULL, &start_copying, (void*)&m_copier);
    if (r != 0) {
        perror("Cannot create backup copy thread.");
        abort();
     }
}


//////////////////////////////////////////////////////////////////////////////
//
// wait_for_copy_to_finish():
//
// Description: 
//
void backup_directory::wait_for_copy_to_finish()
{
    int r = pthread_join(m_thread, NULL);
    if (r) {
        perror("Toku Hot Backup: <COPY> pthread error: ");
    }
}

