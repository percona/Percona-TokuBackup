/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#ifndef BACKUP_COPIER
#define BACKUP_COPIER

#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include "backup.h"
#include "backup_callbacks.h"

#include <stdint.h>
#include <sys/types.h>
#include <vector>
#include <dirent.h>

class backup_copier {
private:
    const char *m_source;
    const char *m_dest;
    int m_copy_error;
    std::vector<char *> m_todo;
    backup_callbacks *m_calls;
    int copy_regular_file(const char *source, const char *dest, off_t file_size, uint64_t *total_bytes_backed_up, const uint64_t total_files_backed_up)  __attribute__((warn_unused_result));
    int add_dir_entries_to_todo(DIR *dir, const char *file)  __attribute__((warn_unused_result));
    void cleanup(void);
public:
    backup_copier(backup_callbacks *calls);
    void set_directories(const char *source, const char *dest);
    void set_error(int error);
    int do_copy() __attribute__((warn_unused_result)) __attribute__((warn_unused_result)); // Returns the error code (not in errno)
    int copy_stripped_file(const char *file, uint64_t *total_bytes_backed_up, const uint64_t total_files_backed_up) __attribute__((warn_unused_result)); // Returns the error code (not in errno)
    int copy_full_path(const char *source, const char* dest, const char *file, uint64_t *total_bytes_backed_up, const uint64_t total_files_backed_up) __attribute__((warn_unused_result)); // Returns the error code (not in errno)
    int copy_file_data(int srcfd, int destfd, const char *source_path, const char *dest_path, off_t source_file_size, uint64_t *total_bytes_backed_up, const uint64_t total_files_backed_up)  __attribute__((warn_unused_result));
};

#endif // End of header guardian.
