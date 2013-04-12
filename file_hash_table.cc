/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id: file_descriptor_map.h 54931 2013-03-29 19:51:23Z bkuszmaul $"

#include <string.h>
#include <pthread.h>

#include "source_file.h"
#include "file_hash_table.h"

////////////////////////////////////////////////////////
//
pthread_mutex_t file_hash_table::m_mutex = PTHREAD_MUTEX_INITIALIZER;

////////////////////////////////////////////////////////
//
file_hash_table::file_hash_table() : m_count(0)
{
    for (int i = 0; i < BUCKET_MAX; ++i) {
        m_table[i] = NULL;
    }
}

////////////////////////////////////////////////////////
//
file_hash_table::~file_hash_table() {
    for (int i=0; i < BUCKET_MAX; i++) {
        while (source_file *head = m_table[i]) {
            m_table[i] = head->next();
            delete head;
        }
    }
}

////////////////////////////////////////////////////////
//
source_file* file_hash_table::get_or_create_locked(const char * const file_name)
{
    source_file * file = NULL;
    int unlock_r = 0;
    int r = this->lock();
    if (r != 0) {
        goto final_out;
    }

    file = this->get(file_name);
    if (file == NULL) {
        file = new source_file(file_name);
        r = file->init();
        if (r != 0) {
            delete file;
            file = NULL;
            goto unlock_out;
        }
        
        this->put(file);
    }
    
    file->add_reference();

unlock_out:
    unlock_r = this->unlock();
    if (unlock_r != 0) {
        if (r != 0) {
            delete file;
        }

        file = NULL;
    }

final_out:
    return file;
}

////////////////////////////////////////////////////////
//
source_file* file_hash_table::get(const char * const full_file_path) const
{
    int hash_index = this->hash(full_file_path);
    source_file *file_found = m_table[hash_index];
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
    unsigned int sum = 0;
    for (int i = 0; i < length; ++i) {
        sum += (unsigned int)file[i];
    }

    int result = sum % file_hash_table::BUCKET_MAX;
    return result;
}

////////////////////////////////////////////////////////
//
void file_hash_table::insert(source_file * const file, int hash_index)
        // It's OK to insert the same file repeatedly (in which case the table is not modified)
{
    source_file *current = m_table[hash_index];
    while (current) {
        if (current == file) return;
        current = current->next();
    }
    file->set_next(m_table[hash_index]);
    m_table[hash_index] = file;
    m_count++;
}

////////////////////////////////////////////////////////
//
void file_hash_table::remove(source_file * const file)
{
    int hash_index = this->hash(file->name());
    source_file *current = m_table[hash_index];
    source_file *previous = NULL;
    while (current != NULL) {
        int result = strcmp(current->name(), file->name());
        if (result == 0) {
            // Remove the entry.
            source_file *next = current->next();
            if (previous != NULL) {
                previous->set_next(next);                
            } else {
                m_table[hash_index] = NULL;
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
int file_hash_table::try_to_remove_locked(source_file * const file)
{
    int r = this->lock();
    if (r != 0) {
        goto error_out;
    }
    
    this->try_to_remove(file);
    r = this->unlock();
error_out:
    return r;
}

////////////////////////////////////////////////////////
//
void file_hash_table::try_to_remove(source_file * const file)
{
    file->remove_reference();
    if (file->get_reference_count() == 0) {
        this->remove(file);
        delete file;
    }
}

////////////////////////////////////////////////////////
//
// This must do a number of things:
// 1. It must grab some kind of write lock, this will wait until all other capture manipulations are complete.
// 2. Then it must remove it from the hash table.
// 3. It must change its name (free old name, use new name);
//
int file_hash_table::rename_locked(const char *original_name, const char *new_name)
{
    source_file * target = NULL;
    int r = this->lock();
    if (r != 0) {
        // Fatal pthread error.
        goto final_out;
    }

    target = this->get(original_name);
    if (target != NULL) {
        r = this->rename(target, new_name);
        if (r != 0) {
            goto unlock_table_out;
        }
    }

unlock_table_out:

    r = this->unlock();

final_out:    
    return r;
}

////////////////////////////////////////////////////////
//
int file_hash_table::rename(source_file * target, const char *new_name)
{
    int r = target->name_write_lock();
    if (r != 0) {
        goto out;
    }
    
    this->remove(target);
    r = target->rename(new_name);
    if (r != 0) {
        goto out;
    }
    
    this->put(target);
    
    r = target->name_unlock();
out:
    return r;
}

////////////////////////////////////////////////////////
//
int file_hash_table::size(void) const
{
    return m_count;
}

////////////////////////////////////////////////////////
//
int file_hash_table::lock(void)
{
    return pthread_mutex_lock(&m_mutex);
}

////////////////////////////////////////////////////////
//
int file_hash_table::unlock(void)
{
    return pthread_mutex_unlock(&m_mutex);
}


