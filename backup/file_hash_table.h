/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
/*======
This file is part of Percona TokuBackup.

Copyright (c) 2006, 2015, Percona and/or its affiliates. All rights reserved.

    Percona TokuBackup is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License, version 2,
    as published by the Free Software Foundation.

     Percona TokuBackup is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Percona TokuBackup.  If not, see <http://www.gnu.org/licenses/>.

----------------------------------------

    Percona TokuBackup is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License, version 3,
    as published by the Free Software Foundation.

    Percona TokuBackup is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with Percona TokuBackup.  If not, see <http://www.gnu.org/licenses/>.
======= */
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

    ///////////////////////////////////////////////////////////////////
    //
    // NOTE:
    //
    //  The application may open a file in direct mode, then close it,
    //  then open the same file again in buffered mode.  Hot Backup
    //  should just use the most recent mode.  
    //
    //  On some file systems (XFS), serialized reads and writes will
    //  not work right when the same file is opened with two file
    //  descriptors, one of which is buffered, and one of which is
    //  direct.  That's a bug in the application if it happens, and we
    //  simply use the most recent mode.
    //
    //  We cache the current flag in source_file object for the copier
    //  to use the most recently set flag/mode.  Hence the extra
    //  argument to get_or_create_locked.
    //
    void get_or_create_locked(const char * const file_name, source_file **file, const int flags) throw();
    void get_or_create_locked(const char * const file_name, source_file **file) throw();
    source_file * get_or_create(const char * const file_name);
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
    int rename_locked(const char *old_src, const char *new_src, const char *new_dst, const char *dst) throw();
    // Does *not* take ownership of the paths.
    // Return values and errors:: On success return 0, otherwise return error number (not in errno), having reported the error to the manager.

  private:
    // The rename method (without a lock) is private to the file_hash_table.
    int rename(source_file * const target, const char *new_name, const char *dest) throw(); // On success return 0, otherwise return error number (not in errno).

  private:
    void lock(void) throw(); // no return results since we assume that mutexes work.
    void unlock(void) throw();
    
    friend class with_file_hash_table_mutex;
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
