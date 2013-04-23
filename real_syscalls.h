/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#ifndef REAL_SYSCALLS_H
#define REAL_SYSCALLS_H

#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include <sys/types.h>

extern pthread_mutex_t backup_manager_mutex;

int call_real_open(const char *file, int oflag, ...) __attribute__((warn_unused_result));
int call_real_close(int fd)  __attribute__((warn_unused_result));
ssize_t call_real_write(int fd, const void *buf, size_t nbyte) __attribute__((warn_unused_result));
ssize_t call_real_read(int fildes, const void *buf, size_t nbyte) __attribute__((warn_unused_result));
ssize_t call_real_pwrite(int fildes, const void *buf, size_t nbyte, off_t offset) __attribute__((warn_unused_result));
off_t call_real_lseek(int fd, off_t offset, int whence) __attribute__((warn_unused_result));
int call_real_ftruncate(int fildes, off_t length) __attribute__((warn_unused_result));
int call_real_truncate(const char *path, off_t length) throw() __attribute__((__nonnull__ (1)))  __attribute__((warn_unused_result));
int call_real_unlink(const char *path) throw() __attribute__((__nonnull__ (1)))  __attribute__((warn_unused_result));
int call_real_rename(const char* oldpath, const char* newpath) __attribute__((warn_unused_result));
int call_real_mkdir(const char *pathname, mode_t mode) throw() __attribute__((__nonnull__ (1))) __attribute__((warn_unused_result));

typedef int (*open_fun_t)(const char *, int, ...);
open_fun_t register_open(open_fun_t new_open);

typedef ssize_t (*pwrite_fun_t)(int, const void *, size_t, off_t);
pwrite_fun_t register_pwrite(pwrite_fun_t new_pwrite); // Effect: The system will call new_pwrite in the future.  The function it would have called is returned (so that the new_pwrite function can use it, if it wants)

typedef ssize_t (*write_fun_t)(int, const void *, size_t);
write_fun_t register_write(write_fun_t new_write);

typedef ssize_t (*read_fun_t)(int, void *, size_t);
read_fun_t register_read(read_fun_t new_read);

typedef off_t (*lseek_fun_t)(int, off_t, int);
lseek_fun_t register_lseek(lseek_fun_t new_lseek);

typedef int (*ftruncate_fun_t)(int, off_t);
ftruncate_fun_t register_ftruncate(ftruncate_fun_t new_ftruncate);

typedef int (*unlink_fun_t)(const char *);
unlink_fun_t register_unlink(unlink_fun_t new_unlink);

typedef int (*rename_fun_t)(const char *, const char *);
rename_fun_t register_rename(rename_fun_t new_rename);

typedef int (*mkdir_fun_t)(const char *, mode_t);
mkdir_fun_t register_mkdir(mkdir_fun_t new_mkdir);

typedef int (*close_fun_t)(int);
close_fun_t register_close(close_fun_t new_close);

#endif // end of header guardian.
