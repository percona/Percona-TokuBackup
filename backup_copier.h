/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#ifndef BACKUP_COPIER
#define BACKUP_COPIER

#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include "backup.h"
#include <sys/types.h>
#include <vector>
#include <dirent.h>

class backup_manager; // need a forward reference for this.

class backup_copier {
private:
    const char *m_source;
    const char *m_dest;
    int m_copy_error;
    std::vector<char *> m_todo;
    int copy_regular_file(const char *source, const char *dest, off_t file_size, backup_manager*, backup_poll_fun_t poll_fun, void*poll_extra, backup_error_fun_t error_fun, void*error_extra);
    int add_dir_entries_to_todo(DIR *dir, const char *file);
public:
    backup_copier();
    void set_directories(const char *source, const char *dest);
    void set_error(int error);
    int get_error(void);
    int do_copy(backup_manager*, backup_poll_fun_t, void*, backup_error_fun_t, void*);
    int copy_stripped_file(const char *file, backup_manager*, backup_poll_fun_t poll_fun, void*poll_extra, backup_error_fun_t error_fun, void*error_extra);
    int copy_full_path(const char *source, const char* dest, const char *file, backup_manager*, backup_poll_fun_t poll_fun, void*poll_extra, backup_error_fun_t error_fun, void*error_extra);
    int copy_file_data(int srcfd, int destfd, const char *source_path, const char *dest_path, off_t source_file_size,
                       backup_manager *,
                       backup_poll_fun_t poll_fun, void*poll_extra, backup_error_fun_t error_fun, void*error_extra);
};

#endif // End of header guardian.
