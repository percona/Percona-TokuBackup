/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#ifndef FILE_HASH_TABLE_H
#define FILE_HASH_TABLE_H

#include <pthread.h>

#include <unordered_map>
#include <string>

class source_file;

class file_hash_table {
public:
    file_hash_table();
    ~file_hash_table();
    source_file * get_or_create_locked(const char * const file_name);
    source_file* get(const char *full_file_path) const;
    void put(source_file * const file);
    void remove(source_file * const file);
    int try_to_remove_locked(source_file * const file);
    void try_to_remove(source_file * const file);
    int rename_locked(const char *original_name, const char *new_name);
    int rename(source_file * const target, const char *new_name);
    int size(void) const;
    int lock(void) __attribute__((warn_unused_result));   // Return 0 on success or an error number.  Reports the error to the backup manager.
    int unlock(void) __attribute__((warn_unused_result)); // Return 0 on success or an error number.  Reports the error to the backup manager.
private:
    std::unordered_map<std::string, source_file *> m_map;
    static pthread_mutex_t m_mutex;
};


#endif // End of header guardian.
