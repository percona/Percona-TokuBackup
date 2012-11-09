/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#include "backup_directory.h"
#include "file_description.h"
#include "backup_debug.h"

#include <string.h>
#include <assert.h>
#include <errno.h>
#include <sys/stat.h>

//class path_creator {
//private:
//    size_t index;
//    const char slash;
//    char directory[256];
//public:
//    path_creator() : index(0), slash('/'), directory("\0") {};
//    const char *get_next_directory(const char *path);
//};
//
/////////////////////////////////////////////////////////////////////////////////
////
//// ():
////
//const char *path_creator::get_next_directory(const char *path) {
//    const char *r = 0;
//    char *postiion = 0;
//    postiion = strchr(path, this->slash);
//    if (postiion) {
//        
//    }
//    /*****************
//    
//    ******************/
//    return r;
//}

static void create_path(const char *path)
{
    // Find directory string
    bool done = false;
    const char slash = '/';
    char directory[256];
    
    while(!done) {
        char *slash_position = NULL;
        slash_position = strchr(path, slash);
        if (slash_position == NULL) {
            done = true;
            continue;
        }
        size_t end = (size_t) (slash_position - path);
        strncpy(directory, path, end);
        int r = mkdir(directory, 0777);
        if (r) { 
            perror("making backup subdirectory failed.");
            // For now, just ignore already existing dir,
            // this is a race between the backup copier
            // and intercepted open() calls.
            if (errno != EEXIST) {
                assert(0);
            }
        }

        path += end + 1;
    }
    
}

///////////////////////////////////////////////////////////////////////////////
//
// start_copying():
//
// Description: 
//
void *start_copying(void * copier)
{
    void *r = 0;
    if (DEBUG) printf(">>> pthread: copy started\n");
    backup_copier *c = (backup_copier*)copier;
    c->start_copy();
    return r;
}

////////////////////////////////////////////////////////////////////////////////
//
// backup_directory():
//
// Description: 
//
backup_directory::backup_directory()
{
}



////////////////////////////////////////////////////////////////////////////////
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
    for (int i = 0; true; i++) {
        if (m_source_dir[i] == 0) return true;
        if (m_source_dir[i] != file[i]) return false;
    }
}

////////////////////////////////////////////////////////////////////////////////
//
void backup_directory::open(file_description * const description)
{    
    // See if the file exists in the backup copy already...
    bool exists = this->does_file_exist(description->name);
    
    if (!exists) {
        this->create_subdirectories(description->name);
    }
}

void backup_directory::create_subdirectories(const char *file)
{
    create_path(file);
}

///////////////////////////////////////////////////////////////////////////////
//
// create() -
//
// Description: 
//
//     ...
//
void backup_directory::create(int fd, const char *file)
{
    // Create new file name based on master file prefix.
    char *new_name = this->translate_prefix(file);
    if (DEBUG) { printf("new_name = %s\n", new_name); }
    
    // See if the path exists in the backup or not.
    // TODO:...
//    const char *path = NULL;
//    
//    bool exists = false;
//    exists = this->does_file_exist(file);
//    
//    if (!exists) {
//        this->create_subdirectories(file);
//    }
}

///////////////////////////////////////////////////////////////////////////////
//
// translate_prefix():
//
// Description: 
//
char* backup_directory::translate_prefix(const char *file)
{
    // TODO: Should we have a copy of these lengths already?
    size_t len_op = strlen(m_source_dir);
    size_t len_np = strlen(m_dest_dir);
    size_t len_s = strlen(file);
    assert(len_op < len_s);
    size_t new_len = len_s - len_op + len_np;
    char *new_string = 0;
    new_string = (char *)calloc(new_len + 1, sizeof(char));
    memcpy(new_string, m_dest_dir, len_np);
    
    // Copy the file name from the directory with the newline at the end.
    memcpy(new_string + len_np, file + len_op, len_s - len_op + 1);
    return new_string;
}

///////////////////////////////////////////////////////////////////////////////
//
bool backup_directory::does_file_exist(const char *file)
{
    bool result = false;
    struct stat sb;
    // We use stat to determine if the file does not exist.
    int r = stat(file, &sb);
    if (r < 0) {
        // We want to catch all other errors.
        if (errno != ENOENT) {
            perror("stat() failed, no backup file information.");
            assert(0);
        }
    } else {
        result = true;
    }
    
    return result;
}

////////////////////////////////////////////////////////////////////////////////
//
// set_directories():
//
// Description: 
//
void backup_directory::set_directories(const char *source, const char *dest)
{
    m_source_dir = source;
    m_dest_dir = dest;
}

///////////////////////////////////////////////////////////////////////////////
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
        assert(0);
     }
}

///////////////////////////////////////////////////////////////////////////////
//
// wait_for_copy_to_finish():
//
// Description: 
//
void backup_directory::wait_for_copy_to_finish()
{
    pthread_join(m_thread, NULL);
}

