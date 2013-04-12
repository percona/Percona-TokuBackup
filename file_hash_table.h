/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id: file_descriptor_map.h 54931 2013-03-29 19:51:23Z bkuszmaul $"

#ifndef FILE_HASH_TABLE_H
#define FILE_HASH_TABLE_H

#include <pthread.h>

class source_file;

class file_hash_table {
public:
    file_hash_table();
    ~file_hash_table();
    source_file * get_or_create_locked(const char * const file_name);
    source_file* get(const char *full_file_path) const;
    void put(source_file * const file);
    int hash(const char * const file) const;
    void insert(source_file * const file, int hash_index); // you may insert the same file more than once.
    void remove(source_file * const file);
    int try_to_remove_locked(source_file * const file);
    void try_to_remove(source_file * const file);
    int rename_locked(const char *original_name, const char *new_name);
    int rename(source_file * const target, const char *new_name);
    int size(void) const;
    int lock(void);
    int unlock(void);
    static const int BUCKET_MAX = 1 << 14;
private:
    source_file *m_table[file_hash_table::BUCKET_MAX];
    int m_count;
    static pthread_mutex_t m_mutex;
};


#endif // End of header guardian.
