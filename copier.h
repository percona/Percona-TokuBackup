/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#ifndef COPIER_H
#define COPIER_H

#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include "backup.h"
#include "backup_callbacks.h"

#include <stdint.h>
#include <sys/types.h>
#include <vector>
#include <dirent.h>
#include <pthread.h>

class file_hash_table;
class source_file;

class copier {
private:
    const char *m_source;
    const char *m_dest;
    std::vector<char *> m_todo;
    backup_callbacks *m_calls;
    file_hash_table * const m_table;
    static pthread_mutex_t m_todo_mutex;
    uint64_t m_total_bytes_backed_up;
    uint64_t m_total_files_backed_up;
    int copy_regular_file(const char *source, const char *dest, off_t file_size)  __attribute__((warn_unused_result));
    int add_dir_entries_to_todo(DIR *dir, const char *file)  __attribute__((warn_unused_result));
    void cleanup(void);
public:
    copier(backup_callbacks *calls, file_hash_table * const table);
    void set_directories(const char *source, const char *dest);
    void set_error(int error);
    int do_copy(void) __attribute__((warn_unused_result)) __attribute__((warn_unused_result)); // Returns the error code (not in errno)
    int copy_stripped_file(const char *file) __attribute__((warn_unused_result)); // Returns the error code (not in errno)
    int copy_full_path(const char *source, const char* dest, const char *file) __attribute__((warn_unused_result)); // Returns the error code (not in errno)
    int copy_file_data(int srcfd, int destfd, const char *source_path, const char *dest_path, source_file * const file, off_t source_file_size)  __attribute__((warn_unused_result)); // Returns the error code (not in errno)
    void add_file_to_todo(const char *file);
    int open_both_files(const char *source, const char *dest, int *srcfd, int *destfd);
};

#endif // End of header guardian.
