/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include <string.h>
#include <pthread.h>
#include <malloc.h>

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
file_hash_table::file_hash_table() : m_count(0),
                                     m_array(new source_file*[1]),
                                     m_size(1)
{
    m_array[0] = NULL;
}

////////////////////////////////////////////////////////
//
file_hash_table::~file_hash_table() {
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
int file_hash_table::get_or_create_locked(const char * const file_name, source_file **file)
{
    int r;
    source_file * source = NULL;
    this->lock();

    source = this->get(file_name);
    if (source == NULL) {
        source = new source_file();
        r = source->init(file_name);
        if (r != 0) {
            delete source;
            source = NULL;
            goto unlock_out;
        }
        
        this->put(source);
    }
    
    source->add_reference();

unlock_out:
    this->unlock();

    *file = source;
    return r;
}

////////////////////////////////////////////////////////
//
source_file* file_hash_table::get(const char * const full_file_path) const
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
void file_hash_table::put(source_file * const file)
{
    int hash_index = this->hash(file->name());
    this->insert(file, hash_index);
}

////////////////////////////////////////////////////////
//
int file_hash_table::hash(const char * const file) const
{
    int length = strlen(file);
    uint64_t the_hash[2];
    MurmurHash3_x64_128(file, length, 0, the_hash);
    return (the_hash[0]+the_hash[1])%m_size;
}

////////////////////////////////////////////////////////
//
void file_hash_table::insert(source_file * const file, int hash_index)
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

void file_hash_table::maybe_resize(void) {
    if (m_size < m_count) {
        source_file **old_array = m_array;
        size_t old_size = m_size;
        m_size = m_size + m_count;
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
void file_hash_table::remove(source_file * const file)
{
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

            m_count--;
            break;
        }
        
        previous = current;
        current = current->next();
    }
}

////////////////////////////////////////////////////////
//
void file_hash_table::try_to_remove_locked(source_file * const file)
{
    this->lock();
    this->try_to_remove(file);
    this->unlock();
}

////////////////////////////////////////////////////////
//
void file_hash_table::try_to_remove(source_file * const file)
{
    file->remove_reference();
    if (file->get_reference_count() == 0) {
        file->try_to_remove_destination();
        this->remove(file);
        delete file;
    }
}

////////////////////////////////////////////////////////
//
int file_hash_table::rename_locked(const char *old_path, const char *new_path, const char *dest_path)
{
    int r;
    source_file * target = NULL;
    this->lock();

    target = this->get(old_path);
    if (target != NULL) {
        r = this->rename(target, new_path, dest_path);
    }
    this->unlock();
    return r;
}

////////////////////////////////////////////////////////
//
int file_hash_table::rename(source_file * target, const char *new_source_name, const char *dest)
{
    destination_file * dest_file = NULL;
    target->name_write_lock();
    
    this->remove(target);
    int r = target->rename(new_source_name);
    if (r != 0) {
        goto unlock_out;
    }

    // If the destination file is NOT NULL, then there is an active
    // backup session in progress.  The session, or a session-enabled
    // open()/create() call, will have allocated the destination_file
    // object in the source_file object already.  If this
    // destination_file object reference is NULL, we do not need to
    // update the destination file; no backup session is in progress,
    // so there is no need to rename a file that does not yet exist.
    dest_file = target->get_destination();
    if (dest_file != NULL) {
        r = dest_file->rename(dest);
        if (r != 0) {
            goto unlock_out;
        }
    } else {
    }

    this->put(target);

unlock_out:
    target->name_unlock();
    return r;
}

////////////////////////////////////////////////////////
//
int file_hash_table::size(void) const
{
    return m_count;
}

////////////////////////////////////////////////////////
// Description: See file_hash_table.h.
void file_hash_table::lock(void) {
    pmutex_lock(&m_mutex);
}

////////////////////////////////////////////////////////
// Description: See file_hash_table.h.
void file_hash_table::unlock(void) {
    pmutex_unlock(&m_mutex);
}


