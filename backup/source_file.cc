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

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "backup_debug.h"
#include "check.h"
#include "manager.h"
#include "mutex.h"
#include "real_syscalls.h"
#include "rwlock.h"
#include "source_file.h"

#if defined(PAUSE_POINTS_ON)
#define PAUSE(number) while(HotBackup::should_pause(number)) { sleep(2); } //printf("Resuming from Pause Point.\n");
#else
#define PAUSE(number)
#endif

////////////////////////////////////////////////////////
//
source_file::source_file(const char *path) throw()
 : m_full_path(strdup(path)),
   m_next(NULL), 
   m_reference_count(0),
   m_unlinked(false),
   m_destination_file(NULL),
   m_flags(0)
{
    {
        int r = pthread_mutex_init(&m_mutex, NULL);
        check(r==0);
    }
    {
        int r = pthread_cond_init(&m_cond, NULL);
        check(r==0);
    }
    {
        int r = pthread_rwlock_init(&m_name_rwlock, NULL);
        check(r==0);
    }
    {
        int r = pthread_mutex_init(&m_fd_mutex, NULL);
        check(r==0);
    }
}

source_file::~source_file(void) throw() {
    if (m_full_path != NULL) {
        free(m_full_path);
        m_full_path = NULL;
        {
            int r = pthread_mutex_destroy(&m_mutex);
            check(r==0);
        }
        {
            int r = pthread_cond_destroy(&m_cond);
            check(r==0);
        }
        {
            int r = pthread_rwlock_destroy(&m_name_rwlock);
            check(r==0);
        }
        {
            int r = pthread_mutex_destroy(&m_fd_mutex);
            check(r==0);
        }
    }
}

////////////////////////////////////////////////////////
//
const char * source_file::name(void) const throw() {
    return m_full_path;
}

////////////////////////////////////////////////////////
//
source_file * source_file::next(void) const throw() {
    return m_next;
}

////////////////////////////////////////////////////////
//
void source_file::set_next(source_file *next_source) throw() {
    m_next = next_source;
}

static bool ranges_intersect (uint64_t lo0, uint64_t hi0,
                              uint64_t lo1, uint64_t hi1) throw()
// Effect: Return true iff [lo0,hi0)  (the half-open interval from lo0 inclusive to hi0 exclusive) intersects [lo1, hi1).
{
    if (lo0 >= hi0) return false; // range0 is empty
    if (lo1 >= hi1) return false; // range1 is empty
    if (hi0 <= lo1) return false; // range0 is before range1
    if (hi1 <= lo0) return false; // range1 is before range0
    return true;
}

bool source_file::lock_range_would_block_unlocked(uint64_t lo, uint64_t hi) const throw() {
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
void source_file::lock_range(uint64_t lo, uint64_t hi) throw() {
    with_mutex_locked ml(&m_mutex, BACKTRACE(NULL));
    while (this->lock_range_would_block_unlocked(lo, hi)) {
        int r = pthread_cond_wait(&m_cond, &m_mutex);
        check(r==0);
    }
    // Got here, we don't intersect any of the ranges.
    struct range new_range = {lo,hi};
    m_locked_ranges.push_back((struct range)new_range);
}


////////////////////////////////////////////////////////
//
int source_file::unlock_range(uint64_t lo, uint64_t hi) throw() {
    with_mutex_locked ml(&m_mutex, BACKTRACE(NULL));
    size_t size = m_locked_ranges.size();
    for (size_t i=0; i<size; i++) {
        if (m_locked_ranges[i].lo == lo &&
            m_locked_ranges[i].hi == hi) {
            m_locked_ranges[i] = m_locked_ranges[size-1];
            m_locked_ranges.pop_back();
            {
                int r = pthread_cond_broadcast(&m_cond);
                check(r==0);
            }
            return 0;
        }
    }
    // No such range.
    the_manager.fatal_error(EINVAL, "Range doesn't exist at %s:%d", __FILE__, __LINE__);
    return EINVAL;
}

////////////////////////////////////////////////////////
//
void source_file::name_write_lock(void) throw() {
    prwlock_wrlock(&m_name_rwlock);
}

////////////////////////////////////////////////////////
//
void source_file::name_read_lock(void) throw() {
    prwlock_rdlock(&m_name_rwlock);
}

////////////////////////////////////////////////////////
//
void source_file::name_unlock(void) throw() {
    prwlock_unlock(&m_name_rwlock);
}

////////////////////////////////////////////////////////
//
int source_file::rename(const char * new_name) throw() {
    int r = 0;
    free(m_full_path);
    m_full_path = call_real_realpath(new_name, NULL);
    if (m_full_path == NULL) {
        r = errno;
    }

    return r;
}

////////////////////////////////////////////////////////
//
void source_file::add_reference(void) throw() {
    __sync_fetch_and_add(&m_reference_count, 1);
}

////////////////////////////////////////////////////////
//
void source_file::remove_reference(void) throw() {
    // TODO.  How can the code that decremented a reference count only if it was positive be right?  Under what conditions could someone be decrementing a refcount when they don't know that it's positive?
    check(m_reference_count>0);
    __sync_fetch_and_add(&m_reference_count, -1);
}

////////////////////////////////////////////////////////
//
unsigned int source_file::get_reference_count(void) const throw() {
    return m_reference_count;
}

////////////////////////////////////////////////////////
//
void source_file::unlink() throw() {
    m_unlinked = true;
}

////////////////////////////////////////////////////////
//
destination_file * source_file::get_destination(void) const throw() {
    return m_destination_file;
}

////////////////////////////////////////////////////////
//
void source_file::set_destination(destination_file *destination) throw() {
    m_destination_file = destination;
}

////////////////////////////////////////////////////////
//
void source_file::try_to_remove_destination(void) throw() {
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
int source_file::try_to_create_destination_file(const char *full_path) throw() {
    if (m_unlinked == true) {
        return 0;
    }

    if (m_destination_file != NULL) {
        return 0;
    }

    PAUSE(HotBackup::OPEN_DESTINATION_FILE);

    // Create the file on disk using the given path, though it may
    // already exist.
    int fd = call_real_open(full_path, O_RDWR | O_CREAT, 0777);
    if (fd < 0) {
        return errno;
    }

    m_destination_file = new destination_file(fd, full_path);
    return 0;
}

////////////////////////////////////////////////////////
//
void source_file::set_flags(const int flags)
{
    with_source_file_fd_lock fdl(this);
    m_flags = flags;
}

////////////////////////////////////////////////////////
//
bool source_file::direct_io_flag_is_set(void) const
{
    return (m_flags & O_DIRECT) != 0;
}

////////////////////////////////////////////////////////
//
bool source_file::locked_direct_io_flag_is_set(void)
{
    with_source_file_fd_lock fdl(this);
    return direct_io_flag_is_set();
}

////////////////////////////////////////////////////////
//
bool source_file::given_flags_are_different(const int flags)
{
    if ((flags & O_DIRECT) == (m_flags & O_DIRECT)) {
        return false;
    }

    return true;
}

////////////////////////////////////////////////////////
//
void source_file::fd_lock(void) throw()
{
    pmutex_lock(&m_fd_mutex);
}

////////////////////////////////////////////////////////
//
void source_file::fd_unlock(void) throw()
{
    pmutex_unlock(&m_fd_mutex);
}

// Instantiate the templates we need
template class std::vector<range>;
