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

#ifndef REAL_SYSCALLS_H
#define REAL_SYSCALLS_H

#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include <sys/types.h>

extern pthread_mutex_t backup_manager_mutex;

int call_real_open(const char *file, int oflag, ...) throw() __attribute__((warn_unused_result));
int call_real_close(int fd) throw()  __attribute__((warn_unused_result));
ssize_t call_real_write(int fd, const void *buf, size_t nbyte) throw() __attribute__((warn_unused_result));
ssize_t call_real_read(int fildes, const void *buf, size_t nbyte) throw() __attribute__((warn_unused_result));
ssize_t call_real_pwrite(int fildes, const void *buf, size_t nbyte, off_t offset) throw() __attribute__((warn_unused_result));
off_t call_real_lseek(int fd, off_t offset, int whence) throw() __attribute__((warn_unused_result));
int call_real_ftruncate(int fildes, off_t length) throw() __attribute__((warn_unused_result));
int call_real_truncate(const char *path, off_t length) throw() __attribute__((__nonnull__ (1)))  __attribute__((warn_unused_result));
int call_real_unlink(const char *path) throw() __attribute__((__nonnull__ (1)))  __attribute__((warn_unused_result));
int call_real_rename(const char* oldpath, const char* newpath) throw() __attribute__((warn_unused_result));
int call_real_mkdir(const char *pathname, mode_t mode) throw() __attribute__((__nonnull__ (1))) __attribute__((warn_unused_result));
char *call_real_realpath(const char *file_name, char *resolved_name) throw() __attribute__((__nonnull__ (1))) __attribute__((warn_unused_result));

typedef int (*open_fun_t)(const char *, int, ...);
open_fun_t register_open(open_fun_t new_open) throw();

typedef ssize_t (*pwrite_fun_t)(int, const void *, size_t, off_t);
pwrite_fun_t register_pwrite(pwrite_fun_t new_pwrite) throw(); // Effect: The system will call new_pwrite in the future.  The function it would have called is returned (so that the new_pwrite function can use it, if it wants)

typedef ssize_t (*write_fun_t)(int, const void *, size_t);
write_fun_t register_write(write_fun_t new_write) throw();

typedef ssize_t (*read_fun_t)(int, void *, size_t);
read_fun_t register_read(read_fun_t new_read) throw();

typedef off_t (*lseek_fun_t)(int, off_t, int);
lseek_fun_t register_lseek(lseek_fun_t new_lseek) throw();

typedef int (*ftruncate_fun_t)(int, off_t);
ftruncate_fun_t register_ftruncate(ftruncate_fun_t new_ftruncate) throw();

typedef int (*unlink_fun_t)(const char *);
unlink_fun_t register_unlink(unlink_fun_t new_unlink) throw();

typedef int (*rename_fun_t)(const char *, const char *);
rename_fun_t register_rename(rename_fun_t new_rename) throw();

typedef int (*mkdir_fun_t)(const char *, mode_t);
mkdir_fun_t register_mkdir(mkdir_fun_t new_mkdir) throw();

typedef int (*close_fun_t)(int);
close_fun_t register_close(close_fun_t new_close) throw();

typedef char* (*realpath_fun_t)(const char *, char *);
realpath_fun_t register_realpath(realpath_fun_t new_realpath) throw();

#endif // end of header guardian.
