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

#include <dlfcn.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include "backup_helgrind.h"

#include "real_syscalls.h"
#include "mutex.h"

static pthread_mutex_t dlsym_mutex = PTHREAD_MUTEX_INITIALIZER;

volatile bool backup_system_is_dead = false;

template <class T> static void dlsym_set(T *ptr, const char *name)
// Effect: lookup up NAME using dlsym, and store the result into *PTR.  Do it atomically, using dlsym_mutex for mutual exclusion.
// Rationale:  There were a whole bunch of these through this code, and they all are the same except the type.  Rather than
//   programming it with macros, I do it in a type-safe way with templates. -Bradley
{
    TOKUBACKUP_VALGRIND_HG_DISABLE_CHECKING(ptr, sizeof(*ptr));
    if (*ptr==NULL) {
        pmutex_lock(&dlsym_mutex); // if things go wrong, what can we do?  We probably cannot even report it.
        if (*ptr==NULL) {
            // the pointer is still NULL, so do the set,  otherwise someone else changed it while I held the pointer.
            T ptr_local = (T)(dlsym(RTLD_NEXT, name));
            // If an error occured, we are hosed, and the system cannot run.  We cannot run even in a degraded mode.  I guess we'll just take the segfault when the call takes place.
            bool did_it __attribute__((__unused__)) = __sync_bool_compare_and_swap(ptr, NULL, ptr_local);
            // If the did_it is false, what can we do.  Try to continue.
        }
        pmutex_unlock(&dlsym_mutex); // if things go wrong, what can we do?  We probably cannot even report it.    Try to continue.
    }
}

template <class T> static void dlvsym_set(T *ptr, const char *name, const char *libname)
// Effect: lookup up NAME using dlsym, and store the result into *PTR.  Do it atomically, using dlsym_mutex for mutual exclusion.
// Rationale:  There were a whole bunch of these through this code, and they all are the same except the type.  Rather than
//   programming it with macros, I do it in a type-safe way with templates. -Bradley
{
    TOKUBACKUP_VALGRIND_HG_DISABLE_CHECKING(ptr, sizeof(*ptr));
    if (*ptr==NULL) {
        pmutex_lock(&dlsym_mutex); // if things go wrong, what can we do?  We probably cannot even report it.
        if (*ptr==NULL) {
            // the pointer is still NULL, so do the set,  otherwise someone else changed it while I held the pointer.
            T ptr_local = (T)(dlvsym(RTLD_NEXT, name, libname));
            // If an error occured, we are hosed, and the system cannot run.  We cannot run even in a degraded mode.  I guess we'll just take the segfault when the call takes place.
            bool did_it __attribute__((__unused__)) = __sync_bool_compare_and_swap(ptr, NULL, ptr_local);
            // If the did_it is false, what can we do.  Try to continue.
        }
        pmutex_unlock(&dlsym_mutex); // if things go wrong, what can we do?  We probably cannot even report it.    Try to continue.
    }
}

// **************************************************************************
//
// These are wrappers for the file system calls.  Our library
// intercepts user calls, and then calls these functions on the user's
// behalf, passing along any necessary arguments.  Our library also
// calls these wrappers to create and edit the backup copies of the
// original data.
//
// **************************************************************************
static int (*real_open)(const char *file, int oflag, ...) = NULL;
int call_real_open(const char *file, int oflag, ...) throw() {
    dlsym_set(&real_open, "open");

    // See if we are creating or just opening the file.
    if (oflag & O_CREAT) {
        va_list op;
        va_start(op, oflag);
        mode_t mode = va_arg(op, mode_t);
        va_end(op);
        return real_open(file, oflag, mode);
    } else {
        return real_open(file, oflag);
    }
}

open_fun_t register_open(open_fun_t f)  throw(){
    dlsym_set(&real_open, "open");
    open_fun_t r = real_open;
    real_open = f;
    return r;
}

static close_fun_t real_close = NULL;

int call_real_close(int fd) throw() {
    dlsym_set(&real_close, "close");
    return real_close(fd);
}

close_fun_t register_close(close_fun_t f) throw() {
    dlsym_set(&real_close, "close");
    close_fun_t r = real_close;
    real_close = f;
    return r;
}

static write_fun_t real_write = NULL;
ssize_t call_real_write(int fd, const void *buf, size_t nbyte) throw() {
    dlsym_set(&real_write, "write");
    return real_write(fd, buf, nbyte);
}
write_fun_t register_write(write_fun_t f) throw() {
    dlsym_set(&real_write, "write");
    write_fun_t r = real_write;
    real_write = f;
    return r;
}

ssize_t call_real_read(int fildes, const void *buf, size_t nbyte) throw() {
    static ssize_t (*real_read)(int fildes, const void *buf, size_t nbyte) = NULL;
    dlsym_set(&real_read, "read");
    return real_read(fildes, buf, nbyte);
}

static pwrite_fun_t real_pwrite = NULL;

ssize_t call_real_pwrite(int fildes, const void *buf, size_t nbyte, off_t offset) throw() {
    dlsym_set(&real_pwrite, "pwrite");
    return real_pwrite(fildes, buf, nbyte, offset);
}
pwrite_fun_t register_pwrite(pwrite_fun_t f) throw() {
    dlsym_set(&real_pwrite, "pwrite");
    pwrite_fun_t r = real_pwrite;
    real_pwrite = f;
    return r;
}

static off_t (*real_lseek)(int, off_t, int) = NULL;
off_t call_real_lseek(int fd, off_t offset, int whence) throw() {
    dlsym_set(&real_lseek, "lseek");
    return real_lseek(fd, offset, whence);
}

lseek_fun_t register_lseek(lseek_fun_t f) throw() {
    dlsym_set(&real_lseek, "lseek");
    lseek_fun_t r = real_lseek;
    real_lseek = f;
    return r;
}

static int (*real_ftruncate)(int fildes,  off_t length) = NULL;
int call_real_ftruncate(int fildes, off_t length) throw() {
    dlsym_set(&real_ftruncate, "ftruncate");
    return real_ftruncate(fildes, length);
}

ftruncate_fun_t register_ftruncate(ftruncate_fun_t f) throw() {
    dlsym_set(&real_ftruncate, "ftruncate");
    ftruncate_fun_t r = real_ftruncate;
    real_ftruncate = f;
    return r;
}

int call_real_truncate(const char *path, off_t length) throw() {
    static int (*real_truncate)(const char *path, off_t length) = NULL;
    dlsym_set(&real_truncate, "truncate");
    return real_truncate(path, length);
}

static unlink_fun_t real_unlink = NULL;
int call_real_unlink(const char *path) throw() {
    dlsym_set(&real_unlink, "unlink");
    return real_unlink(path);
}

unlink_fun_t register_unlink(unlink_fun_t f) throw() {
    dlsym_set(&real_unlink, "unlink");
    unlink_fun_t r = real_unlink;
    real_unlink = f;
    return r;
}

static rename_fun_t real_rename = NULL;
int call_real_rename(const char* oldpath, const char* newpath) throw() {
    dlsym_set(&real_rename, "rename");
    return real_rename(oldpath, newpath);
}

rename_fun_t register_rename(rename_fun_t f) throw() {
    dlsym_set(&real_rename, "rename");
    rename_fun_t r = real_rename;
    real_rename = f;
    return r;
}

static mkdir_fun_t real_mkdir = NULL;

int call_real_mkdir(const char *pathname, mode_t mode) throw() {
    dlsym_set(&real_mkdir, "mkdir");
    return real_mkdir(pathname, mode);
}

mkdir_fun_t register_mkdir(mkdir_fun_t f) throw() {
    dlsym_set(&real_mkdir, "mkdir");
    mkdir_fun_t r = real_mkdir;
    real_mkdir = f;
    return r;
}

static realpath_fun_t real_realpath = NULL;
realpath_fun_t register_realpath(realpath_fun_t f) throw() {
    dlvsym_set(&real_realpath, "realpath", "GLIBC_2.3");
    realpath_fun_t r = real_realpath;
    real_realpath = f;
    return r;
}

char *call_real_realpath(const char *pathname, char *result) throw() {
    dlvsym_set(&real_realpath, "realpath", "GLIBC_2.3");
    return real_realpath(pathname, result);
}
