/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#include <dlfcn.h>
#include <assert.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include <pthread.h>

#include "real_syscalls.h"

static pthread_mutex_t dlsym_mutex = PTHREAD_MUTEX_INITIALIZER;

template <class T> static void dlsym_set(T *ptr, const char *name)
// Effect: lookup up NAME using dlsym, and store the result into *PTR.  Do it atomically, using dlsym_mutex for mutual exclusion.
// Rationale:  There were a whole bunch of these through this code, and they all are the same except the type.  Rather than
//   programming it with macros, I do it in a type-safe way with templates. -Bradley
{
    if (*ptr==NULL) {
        pthread_mutex_lock(&dlsym_mutex);
        if (*ptr==NULL) {
            // the pointer is still NULL, so do the set,  otherwise someone else changed it while I held the pointer.
            T ptr_local = (T)(dlsym(RTLD_NEXT, name));
            assert(ptr_local != NULL);
            bool did_it = __sync_bool_compare_and_swap(ptr, NULL, ptr_local);
            assert(did_it);
        }
        pthread_mutex_unlock(&dlsym_mutex);
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

int call_real_open(const char *file, int oflag, ...) {
    // Put this static decl inside so that someone else cannot use it by mistake.
    static int (*real_open)(const char *file, int oflag, ...) = NULL; 

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

int call_real_close(int fd) {
    static int (*real_close)(int fd) = NULL;
    dlsym_set(&real_close, "close");
    return real_close(fd);
}

ssize_t call_real_write(int fd, const void *buf, size_t nbyte) {
    static ssize_t (*real_write)(int fd, const void *buf, size_t nbyte) = NULL;
    dlsym_set(&real_write, "write");
    return real_write(fd, buf, nbyte);
}

ssize_t call_real_read(int fildes, const void *buf, size_t nbyte) {
    static ssize_t (*real_read)(int fildes, const void *buf, size_t nbyte) = NULL;
    dlsym_set(&real_read, "read");
    return real_read(fildes, buf, nbyte);
}

ssize_t call_real_pwrite(int fildes, const void *buf, size_t nbyte, off_t offset) {
    static ssize_t (*real_pwrite)(int fildes, const void *buf, size_t nbyte, off_t offset) = NULL;
    dlsym_set(&real_pwrite, "pwrite");
    return real_pwrite(fildes, buf, nbyte, offset);
}

off_t call_real_lseek(int fd, off_t offset, int whence) {
    static off_t (*real_lseek)(int, off_t, int) = NULL;
    dlsym_set(&real_lseek, "lseek");
    return real_lseek(fd, offset, whence);
}

int call_real_ftruncate(int fildes, off_t length) {
    static int (*real_ftruncate)(int fildes,  off_t length) = NULL;
    dlsym_set(&real_ftruncate, "ftruncate");
    return real_ftruncate(fildes, length);
}

int call_real_truncate(const char *path, off_t length) {
    static int (*real_truncate)(const char *path, off_t length) = NULL;
    dlsym_set(&real_truncate, "truncate");
    return real_truncate(path, length);
}

int call_real_unlink(const char *path) {
    static int (*real_unlink)(const char* path) = NULL;
    dlsym_set(&real_unlink, "unlink");
    return real_unlink(path);
}

int call_real_rename(const char* oldpath, const char* newpath) {
    static int (*real_rename)(const char *oldpath, const char *newpath) = NULL;
    dlsym_set(&real_rename, "rename");
    return real_rename(oldpath, newpath);
}

int call_real_mkdir(const char *pathname, mode_t mode) {
    static int (*real_mkdir)(const char *pathname, mode_t mode) = NULL;
    dlsym_set(&real_mkdir, "mkdir");
    return real_mkdir(pathname, mode);
}
