/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "glassbox.h"
#include "source_file.h"
#include "manager.h"
#include "mutex.h"
#include "real_syscalls.h"

////////////////////////////////////////////////////////
//
source_file::source_file()
 : m_full_path(NULL),
   m_next(NULL), 
   m_name_rwlock(NULL),
   m_reference_count(0),
   m_mutex(NULL),
   m_cond(NULL),
   m_unlinked(false),
   m_destination_file(NULL)
{
};

source_file::~source_file(void) {
    if (m_full_path != NULL) {
        free(m_full_path);
    }
    if (m_mutex) {
        int r = pthread_mutex_destroy(m_mutex);
        glass_assert(r==0);
        delete m_mutex;
        m_mutex = NULL;
    }
    if (m_cond) {
        int r = pthread_cond_destroy(m_cond);
        glass_assert(r==0);
        delete m_cond;
        m_cond = NULL;
    }
    if (m_name_rwlock) {
        int r = pthread_rwlock_destroy(m_name_rwlock);
        glass_assert(r==0);
        delete m_name_rwlock;
        m_name_rwlock = NULL;
    }
}

int source_file::init(const char *path)
{
    m_full_path = strdup(path);
    if (m_full_path == NULL) {
        int r = errno;
        the_manager.backup_error(r, "Could not allocate memory.");
        return r;
    }

    {
        m_mutex = new pthread_mutex_t;
        int r = pthread_mutex_init(m_mutex, NULL);
        if (r!=0) {
            the_manager.fatal_error(r, "mutex_init at %s:%d", __FILE__, __LINE__);
            delete m_mutex; m_mutex = NULL;
            return r;
        }
    }
    {
        m_cond = new pthread_cond_t;
        int r = pthread_cond_init(m_cond, NULL);
        if (r!=0) {
            the_manager.fatal_error(r, "cond_init at %s:%d", __FILE__, __LINE__);
            ignore(pthread_mutex_destroy(m_mutex));
            delete m_mutex;  m_mutex = NULL;
            delete m_cond;   m_cond  = NULL;
            return r;
        }
    }
    {
        m_name_rwlock = new pthread_rwlock_t;
        int r = pthread_rwlock_init(m_name_rwlock, NULL);
        if (r!=0) {
            ignore(pthread_mutex_destroy(m_mutex));
            ignore(pthread_cond_destroy(m_cond));
            delete m_mutex;       m_mutex       = NULL;
            delete m_cond;        m_cond        = NULL;
            delete m_name_rwlock; m_name_rwlock = NULL;
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
void source_file::set_next(source_file *next_source) {
    m_next = next_source;
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
    pmutex_lock(m_mutex);
    while (this->lock_range_would_block_unlocked(lo, hi)) {
        int r = pthread_cond_wait(m_cond, m_mutex);
        if (r!=0) {
            the_manager.fatal_error(r, "Trying to cond_wait at %s:%d", __FILE__, __LINE__);
            pmutex_unlock(m_mutex);
            return r;
        }
    }
    // Got here, we don't intersect any of the ranges.
    struct range new_range = {lo,hi};
    m_locked_ranges.push_back((struct range)new_range);
    pmutex_unlock(m_mutex);
    return 0;
}


////////////////////////////////////////////////////////
//
int source_file::unlock_range(uint64_t lo, uint64_t hi)
{
    pmutex_lock(m_mutex);
    size_t size = m_locked_ranges.size();
    for (size_t i=0; i<size; i++) {
        if (m_locked_ranges[i].lo == lo &&
            m_locked_ranges[i].hi == hi) {
            m_locked_ranges[i] = m_locked_ranges[size-1];
            m_locked_ranges.pop_back();
            {
                int r = pthread_cond_broadcast(m_cond);
                if (r!=0) {
                    the_manager.fatal_error(r, "Trying to cond_broadcast at %s:%d", __FILE__, __LINE__);
                    pmutex_unlock(m_mutex);
                    return r;
                }
            }
            pmutex_unlock(m_mutex);
            return 0;
        }
    }
    // No such range.
    the_manager.fatal_error(EINVAL, "Range doesn't exist at %s:%d", __FILE__, __LINE__);
    pmutex_unlock(m_mutex);
    return EINVAL;
}

////////////////////////////////////////////////////////
//
int source_file::name_write_lock(void)
{
    int r = pthread_rwlock_wrlock(m_name_rwlock);
    if (r!=0) {
        the_manager.fatal_error(r, "rwlock_rwlock at %s:%d", __FILE__, __LINE__);
    }
    return r;
}

////////////////////////////////////////////////////////
//
int source_file::name_read_lock(void)
{
    int r = pthread_rwlock_rdlock(m_name_rwlock);
    if (r!=0) {
        the_manager.fatal_error(r, "rwlock_rdlock at %s:%d", __FILE__, __LINE__);
    }
    return r;
}

////////////////////////////////////////////////////////
//
int source_file::name_unlock(void)
{
    int r = pthread_rwlock_unlock(m_name_rwlock);
    if (r!=0) {
        the_manager.fatal_error(r, "rwlock_unlock at %s:%d", __FILE__, __LINE__);
    }
    return r;
}

////////////////////////////////////////////////////////
//
int source_file::rename(const char * new_name)
{
    int r = 0;
    free(m_full_path);
    m_full_path = realpath(new_name, NULL);
    if (m_full_path == NULL) {
        r = errno;
    }

    return r;
}

////////////////////////////////////////////////////////
//
void source_file::add_reference(void)
{
    __sync_fetch_and_add(&m_reference_count, 1);
}

////////////////////////////////////////////////////////
//
void source_file::remove_reference(void)
{
    // TODO.  How can the code that decremented a reference count only if it was positive be right?  Under what conditions could someone be decrementing a refcount when they don't know that it's positive?
    glass_assert(m_reference_count>0);
    __sync_fetch_and_add(&m_reference_count, -1);
}

////////////////////////////////////////////////////////
//
unsigned int source_file::get_reference_count(void)
{
    return m_reference_count;
}

////////////////////////////////////////////////////////
//
void source_file::unlink()
{
    m_unlinked = true;
}

////////////////////////////////////////////////////////
//
destination_file * source_file::get_destination(void) const
{
    return m_destination_file;
}

////////////////////////////////////////////////////////
//
void source_file::set_destination(destination_file *destination)
{
    m_destination_file = destination;
}

////////////////////////////////////////////////////////
//
void source_file::try_to_remove_destination(void)
{
    // The only way this function could be called when this
    // source_file object's destination_file reference is NULL is
    // when it was created by the COPY object.
    if (m_destination_file == NULL) {
        return;
    }

    if (m_reference_count > 1) {
        return;
    }

    ignore(m_destination_file->close());
    delete m_destination_file;
    m_destination_file = NULL;
}

////////////////////////////////////////////////////////
//
int source_file::try_to_create_destination_file(char * full_path)
{
    int r = 0;
    if (m_unlinked == true) {
        free((void*)full_path);
        return r;
    }

    if (m_destination_file != NULL) {
        free((void*)full_path);
        return r;
    }

    // Create the file on disk using the given path, though it may
    // already exist.
    int fd = call_real_open(full_path, O_RDWR | O_CREAT, 0777);
    if (fd < 0) {
        r = errno;
        if (r != EEXIST) {
            free((void*)full_path);
            return r;
        }

        fd = call_real_open(full_path, O_RDWR, 0777);
        if (fd < 0) {
            r = errno;
            free((void*)full_path);
            return r;
        }
    }

    m_destination_file = new destination_file(fd, full_path);
    return r;
}

// Instantiate the templates we need
template class std::vector<range>;
