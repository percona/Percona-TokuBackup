/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#ifndef REAL_SYSCALLS_H
#define REAL_SYSCALLS_H

#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include <sys/types.h>

extern pthread_mutex_t backup_manager_mutex;

int call_real_open(const char *file, int oflag, ...);
int call_real_close(int fd);
ssize_t call_real_write(int fd, const void *buf, size_t nbyte);
ssize_t call_real_read(int fildes, const void *buf, size_t nbyte);
ssize_t call_real_pwrite(int fildes, const void *buf, size_t nbyte, off_t offset);
off_t call_real_lseek(int fd, off_t offset, int whence);
int call_real_ftruncate(int fildes, off_t length);
int call_real_truncate(const char *path, off_t length);
int call_real_unlink(const char *path);
int call_real_rename(const char* oldpath, const char* newpath);
int call_real_mkdir(const char *pathname, mode_t mode);

#endif // end of header guardian.
