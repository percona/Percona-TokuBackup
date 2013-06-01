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
    file_hash_table() throw();
    ~file_hash_table() throw();
    void get_or_create_locked(const char * const file_name, source_file **file) throw();
    source_file* get(const char *full_file_path) const throw();
    void put(source_file * const file) throw();
    int hash(const char * const file) const throw();
    void insert(source_file * const file, int hash_index) throw(); // you may insert the same file more than once.
    void remove(source_file * const file) throw();
    void try_to_remove_locked(source_file * const file) throw();
    void try_to_remove(source_file * const file) throw();

    // These methods rename at least the source_file object and
    // reinsert it into our hash table.  If there is a
    // destination_file object, that file is also renamed.
    int rename_locked(const char *old_name, const char *new_name, const char *dest) throw(); // On success return 0, otherwise return error number (not in errno).
    int rename(source_file * const target, const char *new_name, const char *dest) throw(); // On success return 0, otherwise return error number (not in errno).

    void lock(void) throw(); // no return results since we assume that mutexes work.
    void unlock(void) throw();
private:
    size_t m_count;
    source_file **m_array;
    size_t m_size;
    static pthread_mutex_t m_mutex;
    void maybe_resize(void) throw();
};

class with_file_hash_table_mutex {
  private:
    file_hash_table *ht;
  public:
    with_file_hash_table_mutex(file_hash_table *h): ht(h) {
        ht->lock();
    }
    ~with_file_hash_table_mutex(void) {
        ht->unlock();
    }
};


#endif // End of header guardian.
