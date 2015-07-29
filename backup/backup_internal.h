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

#ifndef BACKUP_INTERNAL_H
#define BACKUP_INTERNAL_H

#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include "backup.h"
#include "sys/types.h"
class backup_callbacks; // need a forward reference for this.


unsigned long get_throttle(void) throw();
// Effect: Callback used during a backup session to get current throttle level.

int create_subdirectories(const char*) throw() __attribute__((warn_unused_result));

char *malloc_snprintf(size_t size, const char *format, ...) throw() __attribute__((format (printf, 2, 3)));
// Effect: Return a malloced string of the given size containing the results of doing snprintf(string,size,format,...)
//  No errors occur if the size isn't big enough, instead a properly null-terminated string of at most size is returned without overflowing any buffers.

int open_path(const char *file_path) throw() __attribute__((warn_unused_result));
// Effect: Create a backup path for a given file if it doesn't exist already.

void backup_pause_disable(bool b) throw();

void backup_set_start_copying(bool b) throw(); // When the backup has started and is about to start copying, wait for this boolean to be true (true by default).
bool backup_is_capturing(void) throw();        // Return true if the backup has started capturing.
bool backup_done_copying(void) throw();          // Return true if the backup has finished copying.  This goes true sometime after is_capturing goes true. 
void backup_set_keep_capturing(bool b) throw();
// Effect:  By default, when a backup finishes, it disables capturing.  If before the backup finishes, someone calls backup_set_keep_capturing(true)
//  then the capturing will keep running until someone calls backup_set_capturing(false).
//  This can be called by any thread.
// You must do this sequence:
//    Initially:  start_copying=true, is_capturing=false, keep_capturing=false
// Before the backup thread starts, if you want to put in the pauses:
//    backup_set_start_copying(false);   
// Then you have the application (on the left) and the backup thread on the right
//    
//    applicationthread                  backupthread
//
//                                       done_copying = false
//                                       is_capturing = true
//    while(!is_capturing)
//    keep_capturing=true;
//    start_copying=true;
//                                       while(!start_copying);
//                                       done_copying=true;
//    while(!done_copying);
//    keep_capturing=false;
//                                       while(keep_capturing);
//                                       is_capturing=false;
//    while(!is_capturing);
// and when we are done we ahve start_copying=true, is_capturing=false, keep_capturing=false so we can go again.

static inline void ignore(int a __attribute__((unused))) throw() {}

long long dirsum(const char*dname) throw();

#endif // end of header guardian.
