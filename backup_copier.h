/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#ifndef BACKUP_COPIER
#define BACKUP_COPIER

#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include "backup.h"
#include "backup_callbacks.h"

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
    backup_callbacks &m_calls;
    int copy_regular_file(const char *source, const char *dest, off_t file_size);
    int add_dir_entries_to_todo(DIR *dir, const char *file);
public:
    backup_copier(backup_callbacks &calls);
    void set_directories(const char *source, const char *dest);
    void set_error(int error);
    int get_error(void);
    int do_copy();
    int copy_stripped_file(const char *file);
    int copy_full_path(const char *source, const char* dest, const char *file);
    int copy_file_data(int srcfd, int destfd, const char *source_path, const char *dest_path, off_t source_file_size);
};

#endif // End of header guardian.
