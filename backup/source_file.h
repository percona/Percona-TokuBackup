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
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#ifndef SOURCE_FILE_H
#define SOURCE_FILE_H

#include <stdint.h>

#include "destination_file.h"
#include "description.h"

struct range {
    uint64_t lo, hi;
};

class source_file {
public:
    source_file(const char *path) throw();
    ~source_file(void) throw(); /// the source file will delete the path, if it has been set.
    const char * name(void) const throw();
    source_file *next(void) const throw();
    void set_next(source_file *next) throw();

    void lock_range(uint64_t lo, uint64_t  hi) throw();
    // Effect: Lock the range specified by [lo,hi) (that is lo inclusive to hi exclusive).   Blocks until no locked range intersects [lo,hi).  Use hi==LLONG_MAX to specify the whole file.  No errors can happen (the only possible errors are pthread mutex errors or memory allcoation errors, in which case it's better just to abort).

    int unlock_range(uint64_t lo, uint64_t hi) throw() __attribute__((warn_unused_result));
    // Effect: Unlock the specified range.  Requires that the range is locked (if we notice a problem we'll return EINVAL).  Return 0 or an error number.

    bool lock_range_would_block_unlocked(uint64_t lo, uint64_t hi) const throw();
    // Effect: Return true if lock_range() would block because [lo,hi) intersects some locked range.
    //  This function does not acquire any locks.  We expose it for testing purposes (it should not be used by production code).x

    // Name locking and associated rename call.
  private: // use the RAII-style with_source_file_name_write_lock to grab the lock.
    void name_write_lock(void) throw();
    void name_read_lock(void) throw();
    void name_unlock(void) throw();
  public:
    int rename(const char * new_name) throw(); // return 0 on success, error number on failure (doesn't set errno)

    // Note: These three methods are not inherintly thread safe.
    // They must be protected with a mutex.
    void add_reference(void) throw();
    void remove_reference(void) throw();
    unsigned int get_reference_count(void) const throw();

    void unlink(void) throw();

    // Methods to manage the lifetime of the destination file
    // corresponding to this source file object.  The lifetime of the
    // destination file should be scoped to a particular backup
    // session object lifetime.
    destination_file * get_destination(void) const throw();
    void set_destination(destination_file * destination) throw();
    void try_to_remove_destination(void) throw();
    int try_to_create_destination_file(const char*) throw();

    // This method allows us to change the Direct I/O related flags
    // on the given source file.
    void set_flags(const int flags);
    bool direct_io_flag_is_set(void) const;
    bool locked_direct_io_flag_is_set(void);
    bool given_flags_are_different(const int flags);

private: // Fd locking using RAII-style object with_source_file_fd_lock to grab the lock.
    void fd_lock(void) throw();
    void fd_unlock(void) throw();

private:
    char * m_full_path; // the source_file owns this.
    source_file *m_next;
    pthread_rwlock_t m_name_rwlock;
    unsigned int m_reference_count;

    pthread_mutex_t  m_mutex;
    pthread_cond_t   m_cond;
    std::vector<struct range> m_locked_ranges;

    bool m_unlinked;
    destination_file * m_destination_file;

    pthread_mutex_t  m_fd_mutex;
    int m_flags;

    friend class with_source_file_name_write_lock;
    friend class with_source_file_name_read_lock;
    friend class with_source_file_fd_lock;
};

class with_source_file_name_write_lock {
  private:
    source_file *m_source_file;
  public:
    with_source_file_name_write_lock(source_file *sf): m_source_file(sf) {
        m_source_file->name_write_lock();
    }
    ~with_source_file_name_write_lock(void) {
        m_source_file->name_unlock();
    }
};

class with_source_file_name_read_lock {
  private:
    source_file *m_source_file;
  public:
    with_source_file_name_read_lock(source_file *sf): m_source_file(sf) {
        m_source_file->name_read_lock();
    }
    ~with_source_file_name_read_lock(void) {
        m_source_file->name_unlock();
    }
};

class with_source_file_fd_lock {
  private:
    source_file *m_source_file;
  public:
    with_source_file_fd_lock(source_file *sf) : m_source_file(sf) {
        m_source_file->fd_lock();
    }
    ~with_source_file_fd_lock(void) {
        m_source_file->fd_unlock();
    }
};

#endif // End of header guardian.
