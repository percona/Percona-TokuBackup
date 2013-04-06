/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id: file_descriptor_map.h 54931 2013-03-29 19:51:23Z bkuszmaul $"

#include "string.h"
#include "pthread.h"

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
{
    source_file *current = m_table[hash_index];
    if (current == NULL) {
        m_table[hash_index] = file;
        m_count++;
    } else {
        while(current->next() != NULL) {
            current = current->next();            
        }

        if (current != file) {
            current->set_next(file);
            m_count++;
        }
    }
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
int file_hash_table::size(void) const
{
    return m_count;
}

////////////////////////////////////////////////////////
//
void file_hash_table::lock(void)
{
    pthread_mutex_lock(&m_mutex);
}

////////////////////////////////////////////////////////
//
void file_hash_table::unlock(void)
{
    pthread_mutex_unlock(&m_mutex);
}
