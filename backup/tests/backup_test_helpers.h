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

#ifndef BACKUP_TEST_HELPERS_H
#define BACKUP_TEST_HELPERS_H


#ident "$Id$"

#include "backup_internal.h"
#include <pthread.h>
#include <time.h>

int systemf(const char formatstring, ...); // Effect: run system on the snprintf of the format string and args.


void pass(void);
void fail(void);
void setup_destination(void);   // Make the destination be an empty directory (delete first if needed).
void setup_source(void); // make an empty source dir (delete first if needed).
void setup_directory(char*); // Make an empty directory with the given path string.
void setup_dirs(void); // fill in data in source.
void cleanup_dirs(void);
void read_and_seek(void);
void test_truncate(void);
void set_dir_count(int new_count);
void start_backup_thread_with_funs(pthread_t *thread,
                                   char *src_dir, char *dst_dir,
                                   backup_poll_fun_t poll_fun, void *poll_extra,
                                   backup_error_fun_t error_fun, void *error_extra,
                                   int expect_return_result);
void start_backup_thread_with_funs(pthread_t *thread,
                                   const char *src_dir[], const char *dst_dir[],
                                   backup_poll_fun_t poll_fun, void *poll_extra,
                                   backup_error_fun_t error_fun, void *error_extra,
                                   int expect_return_result);
// Effect: Start doing a backup (on a thread).  The source and dest dirs are as specified.
//  The poll and error funs are as provided.  Expect the
//  backup to return the value of expect_return_result.
//  src_dir and dst_dir are freed by this function (if they are non-null)

void start_backup_thread(pthread_t *thread);
// Effect: start doing backup (on a thread, the thread is returned in *thread).  Use a simple polling function
//   and exect there to be no errors.

void start_backup_thread(pthread_t *thread, char* destination);
// Effect: start doing backup on a seperate thread, creating the
// backup in the given destination directory.

void start_backup_thread_with_exclusion_callback(pthread_t *thread, backup_exclude_copy_fun_t fun, void *extra);
// Effect: start doing backup on a separate thread, but this function
// allows user to also set custom exclusion callback for skipping
// files during a backup operation.

int simple_poll_fun(float progress, const char *progress_string, void *poll_extra);
// A simple polling function.

void finish_backup_thread(pthread_t thread); // wait for backup to finish (pass the thread provided by start_backup_thread()

bool backup_thread_is_done(void); // Tell me that finish_backup_thread is done.


int systemf(const char *formatstring, ...) __attribute__((format (printf, 1, 2))); // Effect: run system() on the snprintf of the args.  Return the exit code.
int openf(int flags, int mode, const char *formatstring, ...)  __attribute__((format(printf, 3, 4))); // Effect: run open(s, flags, mode) where s is gotten by formatting the string.

char *get_src(int dir_index = 0); // returns a malloc'd string for the source directory.  If you call twice you get two different strings.  Requires that the main program defined BACKUP_NAME to be something unique across tests.
char *get_dst(int dir_index = 0); // returns a malloc'd string for the destination directory.

extern int test_main(int, const char *[]); // user code calls this function.

int dummy_poll(float, const char*, void*);
void dummy_error(int, const char*, void*);
unsigned long dummy_throttle(void);

extern int client_n_polls_wait; // poll the first few times fast, and then one of the polls waits for the client to be done, then the polls go normally.
extern volatile int client_done; // set this when it's OK for the poll to return

void start_backup_thread_with_pollwait(pthread_t *thread);

static inline double tdiff(const struct timespec start, const struct timespec end) {
    return end.tv_sec - start.tv_sec + (end.tv_nsec - start.tv_nsec)*1e-9;
}

#include <stdlib.h>
#include <stdio.h>
static inline void check_fun_in_test_helpers(long predicate, const char *expr, const char *file, int line) throw() {
    if (!predicate) {
        fprintf(stderr, "check(%s) failed at %s:%d\n", expr, file, line);
        abort();
    }
}

// Like assert, except it doesn't go away under NDEBUG.
// Do this with a function so that we don't get false answers on branch coverage.
#define check(x) check_fun_in_test_helpers((long)(x), #x, __FILE__, __LINE__)

#endif // End of header guardian.
