/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#ifndef BACKUP_DIRECTORY_H
#define BACKUP_DIRECTORY_H

#include "file_description.h"
#include "backup_copier.h"

#include <pthread.h>
#include <vector>
#include <pthread.h>

class backup_directory
{
private:
    const char *m_source_dir;
    const char *m_dest_dir;
    int m_source_length;
    int m_dest_length;
    std::vector<file_description*> m_descriptions;
    backup_copier m_copier;
    pthread_t m_thread;
public:
    backup_directory();
    file_description* get_file_description(int fd);
    bool is_prefix(const char *file);
    char* translate_prefix(const char *file);
    void add_description(int fd, file_description *description);
    void set_directories(const char *source, const char *dest);
    void start_copy();
    void wait_for_copy_to_finish();
private:
    void grow_fds_array(int fd);
};

#endif // End of header guardian.
