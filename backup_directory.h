/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#ifndef BACKUP_DIRECTORY_H
#define BACKUP_DIRECTORY_H

#include "file_description.h"
#include "file_descriptor_map.h"
#include "backup_copier.h"

#include <pthread.h>
#include <vector>
#include <pthread.h>

class backup_directory
{
private:
    const char *m_source_dir;
    const char *m_dest_dir;
    backup_copier m_copier;
    pthread_t m_thread;

public:
    backup_directory();
    void open_path(const char *file_path);
    bool directories_set();
    bool is_prefix(const char *file);
    char* translate_prefix(const char *file);
    void set_directories(const char *source, const char *dest);
    int set_source_directory(const char *source);
    int set_destination_directory(const char *destination);
    void start_copy();
    void wait_for_copy_to_finish();
    void create_subdirectories(const char *file);
private:
    bool does_file_exist(const char *file);
};

#endif // End of header guardian.
