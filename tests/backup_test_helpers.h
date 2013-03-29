/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#ifndef BACKUP_TEST_HELPERS_H
#define BACKUP_TEST_HELPERS_H

#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include "backup.h"
#include <pthread.h>

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
void start_backup_thread_with_funs(pthread_t *thread,
                                   char *src_dir, char *dst_dir,
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

int simple_poll_fun(float progress, const char *progress_string, void *poll_extra);
// A simple polling function.

void finish_backup_thread(pthread_t thread); // wait for backup to finish (pass the thread provided by start_backup_thread()

int systemf(const char *formatstring, ...) __attribute__((format (printf, 1, 2))); // Effect: run system() on the snprintf of the args.  Return the exit code.
int openf(int flags, int mode, const char *formatstring, ...)  __attribute__((format(printf, 3, 4))); // Effect: run open(s, flags, mode) where s is gotten by formatting the string.

char *get_src(void); // returns a malloc'd string for the source directory.  If you call twice you get two different strings.  Requires that the main program defined BACKUP_NAME to be something unique across tests.
char *get_dst(void); // returns a malloc'd string for the destination directory.

extern int test_main(int, const char *[]); // user code calls this function.

int dummy_poll(float, const char*, void*);
void dummy_error(int, const char*, void*);
unsigned long dummy_throttle(void);

extern int client_n_polls_wait; // poll the first few times fast, and then one of the polls waits for the client to be done, then the polls go normally.
extern volatile int client_done; // set this when it's OK for the poll to return

void start_backup_thread_with_pollwait(pthread_t *thread);

#endif // End of header guardian.
