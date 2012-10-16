/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#include "backup_directory.h"
#include "file_description.h"
#include "backup_debug.h"

#include <string.h>
#include <assert.h>
#include <pthread.h>


void *start_copying(void * copier)
{
    void *r = 0;
    if (DEBUG) printf(">>> pthread: copy started\n");
    backup_copier *c = (backup_copier*)copier;
    c->start_copy();
    return r;
}

backup_directory::backup_directory()
    : m_source_length(0), m_dest_length(0)
{
}

// Returns the file description for the given file descriptor.
file_description* backup_directory::get_file_description(int fd)
{
    file_description *r = 0;
    //    r = this->descriptions.fetch_unchecked(fd);
    r = m_descriptions.at(fd);
    if (r == 0) {
        // TODO: Do we want to support writing to a file when it has
        // not been opened (via backup)? 
        // 1. allocate new description.
        // 2. store it in the array.
        // 3. update its offset based on the current position. (is
        // this thread safe?)
    }

    return r;
}


// Determines if the given file name is within our source directory or not.
bool backup_directory::is_prefix(const char *file)
{
    for (int i = 0; true; i++) {
        if (m_source_dir[i] == 0) return true;
        if (m_source_dir[i] != file[i]) return false;
    }
}

char* backup_directory::translate_prefix(const char *file)
{
    // TODO: Don't we have a copy of these lengths already?
    size_t len_op = strlen(m_source_dir);
    size_t len_np = strlen(m_dest_dir);
    size_t len_s = strlen(file);
    assert(len_op < len_s);
    size_t new_len = len_s - len_op + len_np;
    char *new_string = 0;
    new_string = (char *)calloc(new_len + 1, sizeof(char));
    //char *XMALLOC_N(new_len+1, new_string);
    memcpy(new_string, m_dest_dir, len_np);
    // Copy the file name from the directory with the newline at the end.
    memcpy(new_string + len_np, file + len_op, len_s - len_op + 1);
    return new_string;
}

void backup_directory::add_description(int fd, file_description *description)
{
    this->grow_fds_array(fd);
    std::vector<file_description*>::iterator it;
    it = m_descriptions.begin();
    it += fd;
    m_descriptions.insert(it, description);
}

void backup_directory::set_directories(const char *source, const char *dest)
{
    size_t len_op = strlen(source);
    size_t len_np = strlen(dest);
    m_source_length = len_op;
    m_dest_length = len_np;
    m_source_dir = source;
    m_dest_dir = dest;
}

// Copies all files and subdirectories to the destination.
void backup_directory::start_copy()
{
    int r = 0;
    m_copier.set_directories(m_source_dir, m_dest_dir);
    
    r = pthread_create(&m_thread, NULL, &start_copying, (void*)&m_copier);
    r ? assert(!"pthread failure????!!!!\n") : (void)0;
    //start_copying(&m_copier);
}

void backup_directory::wait_for_copy_to_finish()
{
    pthread_join(m_thread, NULL);
}

void backup_directory::grow_fds_array(int fd)
{
    assert(fd >= 0);
    while(m_descriptions.size() <= fd) {
        m_descriptions.push_back(0);
    }
}
