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


#include "directory_set.h"
#include "raii-malloc.h"
#include "manager.h"
#include "real_syscalls.h"

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

directory_set::directory_set(const int count,
                             const char **sources,
                             const char **destinations)
:m_count(count), m_real_path_successful(false)
{
    m_sources = new const char *[m_count];
    m_destinations = new const char *[m_count];
    for (int i = 0; i < m_count; ++i) {
        m_sources[i] = sources[i];
        m_destinations[i] = destinations[i];
    }
}

directory_set::~directory_set()
{
    if (m_real_path_successful) {
        for (int i = 0; i < m_count; ++i) {
            free(const_cast<char*>(m_sources[i]));
            free(const_cast<char*>(m_destinations[i]));
        }
    }
    
    delete [] m_sources;
    delete [] m_destinations;
}

int directory_set::update_to_full_path(void)
{
    int r = 0;
    int allocated_pairs = 0;
    for (int i = 0; i < m_count; ++i) {
        r = this->update_to_real_path_on_index(i);
        allocated_pairs++;
    }
    
    this->handle_realpath_results(r, allocated_pairs);
    return r;
}

//--------------------------------------------------------
// Complain if:
// 1) the backup directory cannot be stat'd (perhaps because it
// doesn't exist)
// 2) The backup directory is not a directory (perhaps because
// it's a regular file)
// 3) the backup directory can not be opened (maybe permissions?)
// 4) The backpu directory cannot be readdir'd (who knows what
//   that could be)
// 5) The backup directory is not empty (there's a file in there
//   #6542)
// 6) The dir cannot be closedir()'d (who knows...)
//
int directory_set::validate(void) const {
    int r = 0;
    struct stat sbuf;
    for (int i = 0; i < m_count; ++i) {
        r = stat(m_destinations[i], &sbuf);
        if (r != 0) {
            r = errno;
            the_manager.backup_error(r,
                                     "Problem stat()ing backup directory %s",
                                         m_destinations[i]);
            break;
        }

        if (!S_ISDIR(sbuf.st_mode)) {
            r = EINVAL;
                the_manager.backup_error(EINVAL, 
                                         "Backup destination %s is not a directory", 
                                         m_destinations[i]);
                break;
        }
        
        DIR *dir = opendir(m_destinations[i]);
        if (dir == NULL) {
            r = errno;
            the_manager.backup_error(r,
                                     "Problem opening backup directory %s", 
                                         m_destinations[i]);
            break;
        }
        
        r = this->verify_destination_is_empty(i, dir);
        int result = closedir(dir);
        if (result != 0) {
            r = errno;
            the_manager.backup_error(r, 
                                     "Problem closedir()ing backup directory %s",
                                     m_destinations[i]);
            // The dir is already as closed as I can get it.
            // Don't call closedir again, just return.
            break;
        }
    }

    return r;
}

//-----------------------------------------------------------------
// Returns the index of a source directory in the current set, if
// the given file is in a subdirectory of that respective source
// directory.  This will return -1 if the given file is not in any
// source directory in the set.
//
int directory_set::find_index_matching_prefix(const char *file) const {
    // TODO: We have to compare the incoming dir against the
    // correct source directory.  The naive implementation
    // compares the incoming dir against all the currently set
    // source dirs.  The better implementation would get a
    // fixed-length prefix of incoming string/path/file, then
    // instantly look up the hash of that prefix in a hash table
    // of the currently set source dirs.  The problem: What length
    // prefix do we pick?  Many users who want to backup more than
    // one directory will have VERY similary path prefixes.
    // Moreover, users will probably only ever want two
    // directories backed up, so it the naive implementation is
    // probably fine.
    
    int result = -1;
    for (int i = 0; i < m_count; ++i) {
        // TODO: Cache the string length of the full path source dirs.
        int r = strncmp(m_sources[i],
                        file,
                        strlen(m_sources[i]));
        
        // We found a matching prefix.
        if (r == 0) {
            result = i;
            break;
        }
    }
    
    return result;
}

//--------------------------
// Getters...
//
const char *directory_set::source_directory_at(const int index) const {
    if (index >= m_count) {
        return NULL;
    }
    
        return m_sources[index];
}

const char *directory_set::destination_directory_at(const int index) const {
    if (index >= m_count) {
        return NULL;
    }
    
    return m_destinations[index];
}

int directory_set::number_of_directories() const {
    return m_count;
}

//////////////////////
// private methods: //
//////////////////////

//----------------------------------------------------------------
// This mehtod checks that there should be no files, except . and
// .. in the given indexed destination directory.
int directory_set::verify_destination_is_empty(const int index, 
                                               DIR *dir) const {
    int r = 0;
    errno = 0;
    struct dirent const *e = NULL;
    do {
        e = readdir(dir);
        if (e != NULL) { 
            if (strcmp(e->d_name, ".") == 0 ||
                strcmp(e->d_name, "..") == 0) {
                continue;
            }
            
            // This is bad.  The directory should be empty.
            r = EINVAL;
            the_manager.backup_error(r, "Backup directory %s is not empty", 
                                     m_destinations[index]);
            break;
        } else if (errno != 0) {
            r = errno;
            the_manager.backup_error(r, "Problem readdir()ing backup directory %s", 
                             m_destinations[index]);
            break;
        }
    } while(e != NULL);
    
    return r;
}

//------------------------------------------------------------------
// This method frees any previous successful and allocated
// realpath() result strings in the case of a realpath failure.
//
void directory_set::handle_realpath_results(const int r, 
                                            const int allocated_pairs) {
    if (r != 0) {
        // free() the successful realpath() calls.
        for (int i = allocated_pairs; i > 0; --i) {
            free(const_cast<char*>(m_sources[i - 1]));
            free(const_cast<char*>(m_destinations[i - 1]));
        }
        
        m_real_path_successful = false;
    } else {
        m_real_path_successful = true;
    }
}

//------------------------------------------------------------------
// Change the given indexes source and destination directory
// names, to their realpath() equivalent.  Returns an error if
// realpath() returns an error.
//
int directory_set::update_to_real_path_on_index(const int i) {
    int r = 0;
    const char *src = NULL;
    const char *dest = NULL;
    src = call_real_realpath(m_sources[i], NULL);
    if (src == NULL) {
        with_object_to_free<char*> str(malloc_snprintf(strlen(m_sources[i]) + 100, "This backup source directory does not exist: %s", m_sources[i]));
        //TODO: calls->report_error(ENOENT, str.value);
        r = ENOENT;
        goto out;
    }

    dest = call_real_realpath(m_destinations[i], NULL);
    if (dest == NULL) {
        with_object_to_free<char*> str(malloc_snprintf(strlen(m_destinations[i]) + 100, "This backup destination directory does not exist: %s", m_destinations[i]));
        //TODO: calls->report_error(ENOENT, str.value); 
        free(const_cast<char*>(src));
        r = ENOENT;
        goto out;
    }
    
    m_sources[i] = src;
    m_destinations[i] = dest;
 out:
    return r;
}

//-----------------------------------------------------
// TODO: We need to check the following combinations:
// - Each source and destination pair do NOT match.
// - No two sources are the same.
// - No two destinations are the same.
//
// Assumption: There are exactly N pairs of source and destination
// directories.
//
int directory_set::verify_no_two_directories_are_the_same() {
    int r = 0;
    return r;
}
