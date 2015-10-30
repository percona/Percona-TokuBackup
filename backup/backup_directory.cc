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

#include "backup_directory.h"
#include "description.h"
#include "backup_debug.h"
#include "raii-malloc.h"
#include "real_syscalls.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

//////////////////////////////////////////////////////////////////////////////
//
backup_session::backup_session(directory_set *dirs, backup_callbacks *calls, file_hash_table * const file) throw()
    : m_dirs(dirs), m_copier(calls, file)
{
}

//////////////////////////////////////////////////////////////////////////////
//
backup_session::~backup_session() throw() {
}

//////////////////////////////////////////////////////////////////////////////
// Loop through the directory set, copying one directory at a
// time.
//
int backup_session::do_copy() throw() {
    int r = 0;
    for (int i = 0; i < m_dirs->number_of_directories(); ++i) {
        m_copier.set_directories(m_dirs->source_directory_at(i),
                                 m_dirs->destination_directory_at(i));
        r = m_copier.do_copy();
        if (r != 0) {
            break;
        }
    }

    return r;
}

///////////////////////////////////////////////////////////////////////////////
//
// is_prefix():
//
// Description: 
//
//     Determines if the given file name is within our source
// directory or not.
//
bool backup_session::is_prefix(const char *file) throw() {
    // mallocing this to make memcheck happy.  I don't like the extra malloc, but I'm more worried about testability than speed right now. -Bradley
    with_object_to_free<char*> absfile(call_real_realpath(file, NULL));
    if (absfile.value==NULL) return false;
    bool result = this->is_prefix_of_realpath(absfile.value);
    return result;
}

///////////////////////////////////////////////////////////////////////////////
//
bool backup_session::is_prefix_of_realpath(const char *absfile) throw() {
    bool result = false;
    const int index = m_dirs->find_index_matching_prefix(absfile);
    if (index != -1) {
        result = true;
    }

    return result;
}

static int does_file_exist(const char*) throw();

///////////////////////////////////////////////////////////////////////////////
//
// open_path():
//
// Description:
//
//     Creates backup path for given file if it doesn't exist already.
//     Any errors are reported, and an error number is returned.
//
int open_path(const char *file_path) throw() {
    int r = 0;
    // See if the file exists in the backup copy already...
    int exists = does_file_exist(file_path);
    
    if (exists == 0) {
        r = create_subdirectories(file_path);
    }
    
    if(exists == -1) {
        r = -1;
    }
    
    return r;
}


///////////////////////////////////////////////////////////////////////////////
//
// create_subdirectories:
//
// Description:
//
//     Recursively creates all the backup subdirectories 
// required for the given path.
//
int create_subdirectories(const char *path) throw() {
    const char SLASH = '/';
    with_object_to_free<char*> directory(strdup(path));
    char *next_slash = directory.value;
    ++next_slash;

    int r = 0;

    while(next_slash != NULL) {
        // 1. scan for next slash
        while(*next_slash != 0) {
            if(*next_slash == SLASH) {
                break;
            }
            
            ++next_slash;
        }
        
        // 2. if found /0, set to null, break.
        if (*next_slash == 0) {
            next_slash = NULL;
            break;
        }
        
        // 3. turn slash into NULL
        *next_slash = 0;
        
        // 4. mkdir
        if (*directory.value) {
            r = call_real_mkdir(directory.value, 0777);
        
            if(r) {
                //printf("WARN: <CAPTURE>: %s\n", directory);
                // For now, just ignore already existing dir,
                // this is a race between the backup copier
                // and the intercepted open() calls.
                if(errno != EEXIST) {
                    r = errno;
                    goto out;
                } else {
                    // EEXIST is ok
                    r = 0;
                }
            }
        }
        
        // 5. revert slash back to slash char.
        *next_slash = SLASH;
        
        // 6. Advance slash position by 1, to search 
        // past recently found slash, and repeat.
        ++next_slash;
    }
    
out:
    return r;
}


//////////////////////////////////////////////////////////////////////////////
// Effect: See backup_directory.h
char* backup_session::translate_prefix(const char *file) throw() {
    char *absfile = call_real_realpath(file, NULL);
    // TODO: What if call_real_realpath() returns a NULL?  It would segfault.  See #6605.
    char *result = translate_prefix_of_realpath(absfile);
    free(absfile);
    return result;
}

char* backup_session::translate_prefix_of_realpath(const char *absfile) throw() {
    const int index = m_dirs->find_index_matching_prefix(absfile);
    // TODO: #6543 Should we have a copy of these lengths already?
    size_t len_op = strlen(m_dirs->source_directory_at(index));
    size_t len_np = strlen(m_dirs->destination_directory_at(index));
    size_t len_s = strlen(absfile);
    size_t new_len = len_s - len_op + len_np +1;
    char *new_string = (char*)malloc(new_len);
    memcpy(new_string, m_dirs->destination_directory_at(index), len_np);
    
    // Copy the file name from the directory with the newline at the end.
    memcpy(new_string + len_np, absfile + len_op, len_s - len_op + 1);
    return new_string;
}

//////////////////////////////////////////////////////////////////////////////
//
// does_file_exist():
//
// Description:
//
static int does_file_exist(const char *file) throw() {
    int result = 0;
    struct stat sb;
    // We use stat to determine if the file does not exist.
    int r = stat(file, &sb);
    if (r < 0) {
        int error = errno;
        // We want to catch all other errors.
        if (error != ENOENT) {
            perror("Toku Hot Backup:stat() failed, no file information.");
            result = -1;
        }
    } else {
        result = 1;
    }

    return result;
}

///////////////////////////////////////////////////////////////////////////////
//
int backup_session::capture_open(const char *file, char **result) throw() {
    if(!this->is_prefix(file)) {
        *result = NULL;
        return 0;
    }
    
    char *backup_file_name = this->translate_prefix(file);
    if (this->file_is_excluded(backup_file_name)) {
        free(backup_file_name);
        return -1;
    }

    int r = open_path(backup_file_name);
    if (r != 0) {
        // The error has been reported.  Propagate it.
        free(backup_file_name);
        return r;
    }
    
    *result = backup_file_name;
    return 0;
}

///////////////////////////////////////////////////////////////////////////////
//
int backup_session::capture_mkdir(const char *pathname) throw() {
    if(!this->is_prefix(pathname)) {
        return 0;
    }

    with_object_to_free<char*> backup_directory_name(this->translate_prefix(pathname));
    int r = open_path(backup_directory_name.value);
    return r;
}

///////////////////////////////////////////////////////////////////////////////
//
void backup_session::add_to_copy_todo_list(const char *file_path) throw() {
     m_copier.add_file_to_todo(file_path);
}

///////////////////////////////////////////////////////////////////////////////
//
void backup_session::cleanup(void) throw() {
    m_copier.cleanup();
}

bool backup_session::file_is_excluded(const char *backup_file) throw() {
    return m_copier.file_should_be_excluded(backup_file);
}
