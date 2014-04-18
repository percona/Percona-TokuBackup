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
class destination_file;

////////////////////////////////////////////////////////////////////////////////
//
// source_info:
//
// Description:
//
//     This is a container struct to simplify passing necessary source
//     and destination information through the copier class' copy
//     paths.
//
struct source_info {
    int m_fd;
    const char *m_path;
    off_t m_size;
    source_file * m_file;
    int m_flags;
};

////////////////////////////////////////////////////////////////////////////////
//
// copy_result:
//
// Description:
//
//   This is a container for the results from copying regions of a file.
// The 'result' integer represents zero on success, non-zero on failure.
// The number written is the status of the copy: 0 means finished, positive
// means more to copy, and negative means an error.  This is essentially a wrapper
// for setting the return result to errno.
//
struct copy_result {
    copy_result() : m_result(0), m_n_wrote_now(0) {};
    int m_result;
    ssize_t m_n_wrote_now;
};

////////////////////////////////////////////////////////////////////////////////
//
class copier {
  private:
    const char *m_source;
    const char *m_dest;
    std::vector<char *> m_todo;
    backup_callbacks *m_calls;
    file_hash_table * const m_table;
    size_t m_total_written_this_file;
public:
    static pthread_mutex_t m_todo_mutex; // make this public so that we can grab the mutex when creating a copier.
private:
    uint64_t m_total_bytes_backed_up;
    uint64_t m_total_files_backed_up;
    uint64_t m_total_bytes_to_back_up; // the number of files that we will need to back up. This is used for the polling callback.
    int copy_regular_file(source_info src_info, const char *dest) throw()  __attribute__((warn_unused_result));
    int copy_using_source_info(source_info src_info, const char *dest) throw();
    int create_destination_and_copy(source_info src_info, const char *dest) throw();
    int add_dir_entries_to_todo(DIR *dir, const char *file) throw() __attribute__((warn_unused_result));
    int possibly_sleep_or_abort(source_info src_info, ssize_t total_written_this_file, destination_file * dest, struct timespec starttime) throw() __attribute__((warn_unused_result));
    ssize_t copy_file_range(source_info src_info, char * buf, size_t buf_size, char *poll_string, size_t poll_string_size, size_t & total_written_this_file) throw() __attribute__((warn_unused_result));
    copy_result open_and_lock_file_then_copy_range(source_info src_info, char *buf, size_t buf_size,char *poll_string,size_t poll_string_size) throw() __attribute__((warn_unused_result));
    copy_result copy_file_range(source_info src_info, char * buf, size_t buf_size, char *poll_string, size_t poll_string_size) throw() __attribute__((warn_unused_result));
public:
    copier(backup_callbacks *calls, file_hash_table * const table) throw();
    void set_directories(const char *source, const char *dest) throw();
    void set_error(int error) throw();
    int do_copy(void) throw() __attribute__((warn_unused_result)) __attribute__((warn_unused_result)); // Returns the error code (not in errno)
    int copy_stripped_file(const char *file) throw() __attribute__((warn_unused_result)); // Returns the error code (not in errno)
    int copy_full_path(const char *source, const char* dest, const char *file) throw() __attribute__((warn_unused_result)); // Returns the error code (not in errno)
    int copy_file_data(source_info src_info) throw() __attribute__((warn_unused_result)); // Returns the error code (not in errno)
    void add_file_to_todo(const char *file) throw();
    int open_both_files(const char *source, const char *dest, int *srcfd, int *destfd) throw();
    void cleanup(void) throw();
};

#endif // End of header guardian.
