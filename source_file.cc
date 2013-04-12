/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id: file_descriptor_map.h 54931 2013-03-29 19:51:23Z bkuszmaul $"

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "source_file.h"

////////////////////////////////////////////////////////
//
source_file::source_file(const char * const path) : m_full_path(strdup(path)), m_next(NULL), m_reference_count(0) 
{
};

source_file::~source_file(void) {
    free(m_full_path);
}

int source_file::init(void)
{
    {
        int r = pthread_mutex_init(&m_mutex, NULL);
        if (r!=0) {
            return r;
        }
    }
    {
        int r = pthread_cond_init(&m_cond, NULL);
        if (r!=0) {
            pthread_mutex_destroy(&m_mutex); // don't worry about an error here.
            return r;
        }
    }
    {
        int r = pthread_rwlock_init(&m_name_rwlock, NULL);
        if (r!=0) {
            pthread_mutex_destroy(&m_mutex); // don't worry about an error here.
            pthread_cond_destroy(&m_cond); // don't worry about an error here.
            return r;
        }
                                  
    }
    return 0;
}

////////////////////////////////////////////////////////
//
const char * source_file::name(void)
{
    return m_full_path;
}

////////////////////////////////////////////////////////
//
source_file * source_file::next(void)
{
    return m_next;
}

////////////////////////////////////////////////////////
//
void source_file::set_next(source_file *next)
{
    m_next = next;
}

static bool ranges_intersect (uint64_t lo0, uint64_t hi0,
                              uint64_t lo1, uint64_t hi1)
// Effect: Return true iff [lo0,hi0)  (the half-open interval from lo0 inclusive to hi0 exclusive) intersects [lo1, hi1).
{
    if (lo0 >= hi0) return false; // range0 is empty
    if (lo1 >= hi1) return false; // range1 is empty
    if (hi0 <= lo1) return false; // range0 is before range1
    if (hi1 <= lo0) return false; // range1 is before range0
    return true;
}

bool source_file::lock_range_would_block_unlocked(uint64_t lo, uint64_t hi) {
    size_t size = m_locked_ranges.size();    
    for (size_t i = 0; i<size; i++) {
        if (ranges_intersect(m_locked_ranges[i].lo, m_locked_ranges[i].hi,
                             lo, hi)) {
            return true;
        }
    }
    return false;
}

////////////////////////////////////////////////////////
//
int source_file::lock_range(uint64_t lo, uint64_t hi)
{
    {
        int r = pthread_mutex_lock(&m_mutex);
        if (r!=0) {
            return r;
        }
    }
    while (this->lock_range_would_block_unlocked(lo, hi)) {
        int r = pthread_cond_wait(&m_cond, &m_mutex);
        if (r!=0) {
            pthread_mutex_unlock(&m_mutex); // ignore any error.
            return r;
        }
    }
    // Got here, we don't intersect any of the ranges.
    struct range new_range = {lo,hi};
    m_locked_ranges.push_back((struct range)new_range);
    {
        int r = pthread_mutex_unlock(&m_mutex);
        return r;
    }
}


////////////////////////////////////////////////////////
//
int source_file::unlock_range(uint64_t lo, uint64_t hi)
{
    {
        int r = pthread_mutex_lock(&m_mutex);
        if (r!=0) {
            return r;
        }
    }
    size_t size = m_locked_ranges.size();
    for (size_t i=0; i<size; i++) {
        if (m_locked_ranges[i].lo == lo &&
            m_locked_ranges[i].hi == hi) {
            m_locked_ranges[i] = m_locked_ranges[size-1];
            m_locked_ranges.pop_back();
            {
                int r = pthread_cond_broadcast(&m_cond);
                if (r!=0) {
                    pthread_mutex_unlock(&m_mutex); // ignore any error from this, since we already have an error.
                    return r;
                }
            }
            return pthread_mutex_unlock(&m_mutex);
        }
    }
    // No such range.
    pthread_mutex_unlock(&m_mutex); // ignore any error from this since we already have an error.
    return EINVAL;
}

////////////////////////////////////////////////////////
//
int source_file::name_write_lock(void)
{
    return pthread_rwlock_wrlock(&m_name_rwlock);
}

////////////////////////////////////////////////////////
//
int source_file::name_read_lock(void)
{
    return pthread_rwlock_rdlock(&m_name_rwlock);
}

////////////////////////////////////////////////////////
//
int source_file::name_unlock(void)
{
    return pthread_rwlock_unlock(&m_name_rwlock);
}

////////////////////////////////////////////////////////
//
int source_file::rename(const char * new_name)
{
    int r = 0;
    free(m_full_path);
    m_full_path = realpath(new_name, NULL);
    if (m_full_path == NULL) {
        r = -1;
    }

    return r;
}

////////////////////////////////////////////////////////
//
void source_file::add_reference(void)
{
    ++m_reference_count;
}

////////////////////////////////////////////////////////
//
void source_file::remove_reference(void)
{
    if (m_reference_count != 0) {
        --m_reference_count;
    }
}

////////////////////////////////////////////////////////
//
unsigned int source_file::get_reference_count(void)
{
    return m_reference_count;
}
