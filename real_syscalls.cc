/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#include <dlfcn.h>
#include <assert.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>

#ifndef NULL
#define NULL 0
#endif

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
int call_real_open(const char *file, int oflag, ...) {
    if (real_open == NULL) {
        // really should have a mutex here to protect this.
        real_open = (int (*)(const char *, int, ...))dlsym(RTLD_NEXT, "open");
        assert(real_open != NULL);
    }

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

static int (*real_close)(int fd) = NULL;
int call_real_close(int fd) {
    if (real_close == NULL) {
        real_close = (int(*)(int))dlsym(RTLD_NEXT, "close");
        assert(real_close != NULL);
    }

    return real_close(fd);
}

static ssize_t (*real_write)(int fd, const void *buf, size_t nbyte) = NULL;
ssize_t call_real_write(int fd, const void *buf, size_t nbyte) {
    if (real_write==NULL) {
        real_write = (ssize_t(*)(int,const void*,size_t))dlsym(RTLD_NEXT, "write");
        assert(real_write != NULL);
    }
    return real_write(fd, buf, nbyte);
}

static int (*real_rename)(const char *oldpath, const char *newpath) = NULL;
int call_real_rename(const char* oldpath, const char* newpath) {
    // TODO: Isn't there a way to set this during backup initialization?
    // Especially if we protect interposition with locking of the backup
    // data structures...
    if (real_rename == NULL) {
        real_rename = (int(*)(const char *,const char *))dlsym(RTLD_NEXT, "rename");
       assert(real_rename != NULL);
    }
    return real_rename(oldpath, newpath);
}
