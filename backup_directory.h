/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#ifndef BACKUP_DIRECTORY_H
#define BACKUP_DIRECTORY_H

#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

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
    int open_path(const char *file_path);
    bool directories_set();
    bool is_prefix(const char *file);
    char* translate_prefix(const char *file);
    int set_directories(const char *source, const char *dest, backup_poll_fun_t poll_fun, void *poll_extra, backup_error_fun_t error_fun, void *error_extra) __attribute__((warn_unused_result));
    int set_source_directory(const char *source);
    int set_destination_directory(const char *destination);
    int do_copy(backup_manager *manage, backup_poll_fun_t poll_fun, void *poll_extra, backup_error_fun_t error_fun, void *error_extra) __attribute__((warn_unused_result));
    void abort_copy(void);
    void create_subdirectories(const char *file);
private:
    int does_file_exist(const char *file);
};

#endif // End of header guardian.
