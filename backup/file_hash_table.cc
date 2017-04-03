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

#ident "$Id$"

#include <string.h>
#include <pthread.h>
#include <malloc.h>
#include <stdio.h>
#include <assert.h>

#include "source_file.h"
#include "file_hash_table.h"
#include "manager.h"
#include "mutex.h"
#include "MurmurHash3.h"

////////////////////////////////////////////////////////
//
pthread_mutex_t file_hash_table::m_mutex = PTHREAD_MUTEX_INITIALIZER;

////////////////////////////////////////////////////////
//
file_hash_table::file_hash_table() throw() : m_count(0),
                                             m_array(new source_file*[1]),
                                             m_size(1)
{
    m_array[0] = NULL;
}

////////////////////////////////////////////////////////
//
file_hash_table::~file_hash_table() throw() {
    for (size_t i=0; i < m_size; i++) {
        while (source_file *head = m_array[i]) {
            m_array[i] = head->next();
            delete head;
        }
    }
    delete[] m_array;
}

////////////////////////////////////////////////////////
//
void file_hash_table::get_or_create_locked(const char * const file_name, source_file **file, const int flags) throw() {
    this->lock();
    source_file * source = this->get_or_create(file_name);
    source->set_flags(flags);
    this->unlock();
    *file = source;
}

////////////////////////////////////////////////////////
//
void file_hash_table::get_or_create_locked(const char * const file_name, source_file **file) throw() {
    this->lock();
    source_file * source = this->get_or_create(file_name);
    this->unlock();
    *file = source;
}

////////////////////////////////////////////////////////
//
source_file * file_hash_table::get_or_create(const char * const file_name) {
    source_file * source = this->get(file_name);
    if (source == NULL) {
        source = new source_file(file_name);
        this->put(source);
    }

    source->add_reference();
    return source;
}

////////////////////////////////////////////////////////
//
source_file* file_hash_table::get(const char * const full_file_path) const throw()
{
    int hash_index = this->hash(full_file_path);
    source_file *file_found = m_array[hash_index];
    while (file_found != NULL) {
        int result = strcmp(full_file_path, file_found->name());
        if (result == 0) {
            break;
        }

        file_found = file_found->next();
    }

    return file_found;
}

////////////////////////////////////////////////////////
//
void file_hash_table::put(source_file * const file) throw() {
    int hash_index = this->hash(file->name());
    this->insert(file, hash_index);
}

////////////////////////////////////////////////////////
//
int file_hash_table::hash(const char * const file) const  throw() {
    assert(m_size);
    int length = strlen(file);
    uint64_t the_hash[2];
    MurmurHash3_x64_128(file, length, 0, the_hash);
    return (the_hash[0]+the_hash[1])%m_size;
}

////////////////////////////////////////////////////////
//
void file_hash_table::insert(source_file * const file, int hash_index)  throw()
        // It's OK to insert the same file repeatedly (in which case the table is not modified)
{
    source_file *current = m_array[hash_index];
    while (current) {
        if (current == file) return;
        current = current->next();
    }
    file->set_next(m_array[hash_index]);
    m_array[hash_index] = file;
    m_count++;
    maybe_resize();
}

void file_hash_table::maybe_resize(void) throw() {
    if (m_size < m_count) {
        source_file **old_array = m_array;
        size_t old_size = m_size;
        m_size = m_size + m_count;
        assert(m_size);
        m_array = new source_file*[m_size];
        for (size_t i=0; i<m_size; i++) {
            m_array[i] = NULL;
        }
        for (size_t i=0; i<old_size; i++) {
            while (1) {
                source_file *head = old_array[i];
                if (head==NULL) break;
                old_array[i] = head->next();
                int hash_index = this->hash(head->name());
                head->set_next(m_array[hash_index]);
                m_array[hash_index] = head;
            }
        }
        delete[] old_array;
    }
}

////////////////////////////////////////////////////////
//
void file_hash_table::remove(source_file * const file) throw() {
    int hash_index = this->hash(file->name());
    source_file *current = m_array[hash_index];
    source_file *previous = NULL;
    while (current != NULL) {
        int result = strcmp(current->name(), file->name());
        if (result == 0) {
            // Remove the entry.
            source_file *next = current->next();
            if (previous != NULL) {
                previous->set_next(next);                
            } else {
                m_array[hash_index] = next;
            }
            assert(m_count);
            m_count--;
            break;
        }
        
        previous = current;
        current = current->next();
    }
}

////////////////////////////////////////////////////////
//
void file_hash_table::try_to_remove_locked(source_file * const file) throw() {
    this->lock();
    this->try_to_remove(file);
    this->unlock();
}

////////////////////////////////////////////////////////
//
void file_hash_table::try_to_remove(source_file * const file) throw() {
    file->remove_reference();
    if (file->get_reference_count() == 0) {
        file->try_to_remove_destination();
        this->remove(file);
        delete file;
    }
}

////////////////////////////////////////////////////////
//
int file_hash_table::rename_locked(const char * const old_path, const char *new_path, const char *old_dest, const char *dest_path) throw() {
    int r = 0;
    with_file_hash_table_mutex ht(this);
    source_file * target = this->get_or_create(old_path);

    // This path should only be called during an active backup
    // session.  So, there MUST be a destination object by this point
    // in the rename path.
    r = target->try_to_create_destination_file(old_dest);
    if (r == 0) {
        r = this->rename(target, new_path, dest_path);
        this->try_to_remove(target);
    }

    return r;
}

////////////////////////////////////////////////////////
//
int file_hash_table::rename(source_file * target, const char *new_source_name, const char *dest) throw() {
    destination_file * dest_file = NULL;
    with_source_file_name_write_lock sfl(target);
    
    // Since we hash on the file name, we have to remove the
    // soon-to-be-renamed file from the hash table, before we can
    // actually 'rename' the object itself.
    this->remove(target);
    {
        int r = target->rename(new_source_name);
        if (r != 0) {
            // We report our own errors.
            the_manager.backup_error(r, "Could not do target->rename to %s", new_source_name);
            return r;
        }
    }

    // The destination_file should NOT be NULL at this point.
    dest_file = target->get_destination();
    int r = dest_file->rename(dest);
    if (r != 0) {
        the_manager.backup_error(r, "Could not do dest_file->rename to %s", dest);
        return r;
    }

    this->put(target);
    return 0;
}

////////////////////////////////////////////////////////
// Description: See file_hash_table.h.
void file_hash_table::lock(void) throw() {
    pmutex_lock(&m_mutex);
}

////////////////////////////////////////////////////////
// Description: See file_hash_table.h.
void file_hash_table::unlock(void) throw() {
    pmutex_unlock(&m_mutex);
}


