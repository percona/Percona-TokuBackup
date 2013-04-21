/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include <string.h>
#include <pthread.h>

#include "source_file.h"
#include "file_hash_table.h"
#include "manager.h"
#include "mutex.h"

////////////////////////////////////////////////////////
//
pthread_mutex_t file_hash_table::m_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef std::unordered_map<std::string, source_file *> map_t;

////////////////////////////////////////////////////////
//
file_hash_table::file_hash_table() {}

////////////////////////////////////////////////////////
//
file_hash_table::~file_hash_table() {
    while (1) {
        map_t::iterator it = m_map.begin();
        if (it == m_map.end()) break;
        delete it->second;
        m_map.erase(it);
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
        file = new source_file();
        r = file->init(file_name);
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
    map_t::const_iterator got = m_map.find(full_file_path);
    if (got == m_map.end()) {
        return NULL;
    } else {
        return got->second;
    }
}

////////////////////////////////////////////////////////
//
void file_hash_table::put(source_file * const file)
{
    m_map[file->name()] = file;
}

////////////////////////////////////////////////////////
//
void file_hash_table::remove(source_file * const file) {
    m_map.erase(file->name());
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
    return m_map.size();
}

////////////////////////////////////////////////////////
// Description: See file_hash_table.h.
int file_hash_table::lock(void)
{
    return pmutex_lock(&m_mutex);
}

////////////////////////////////////////////////////////
// Description: See file_hash_table.h.
int file_hash_table::unlock(void)
{
    return pmutex_unlock(&m_mutex);
}


