/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ifndef FILE_HASH_TABLE_H
#define FILE_HASH_TABLE_H

#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include <pthread.h>
#include <vector>

class source_file;

class file_hash_table {
public:
    file_hash_table();
    ~file_hash_table();
    int get_or_create_locked(const char * const file_name, source_file **file);
    source_file* get(const char *full_file_path) const;
    void put(source_file * const file);
    int hash(const char * const file) const;
    void insert(source_file * const file, int hash_index); // you may insert the same file more than once.
    void remove(source_file * const file);
    int try_to_remove_locked(source_file * const file);
    void try_to_remove(source_file * const file);

    // These methods rename at least the source_file object and
    // reinsert it into our hash table.  If there is a
    // destination_file object, that file is also renamed.
    int rename_locked(const char *old_name, const char *new_name, const char *dest);
    int rename(source_file * const target, const char *new_name, const char *dest);

    int size(void) const;
    int lock(void) __attribute__((warn_unused_result));   // Return 0 on success or an error number.  Reports the error to the backup manager.
    int unlock(void) __attribute__((warn_unused_result)); // Return 0 on success or an error number.  Reports the error to the backup manager.
private:
    size_t m_count;
    source_file **m_array;
    size_t m_size;
    static pthread_mutex_t m_mutex;
    void maybe_resize(void);
};


#endif // End of header guardian.
