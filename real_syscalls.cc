/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#include <dlfcn.h>
#include <assert.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>

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
int call_real_open(const char *file, int oflag, ...);
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
int call_real_close(int fd);
int call_real_close(int fd) {
    if (real_close == NULL) {
        real_close = (int(*)(int))dlsym(RTLD_NEXT, "close");
        assert(real_close != NULL);
    }

    return real_close(fd);
}

static ssize_t (*real_write)(int fd, const void *buf, size_t nbyte) = NULL;
ssize_t call_real_write(int fd, const void *buf, size_t nbyte);
ssize_t call_real_write(int fd, const void *buf, size_t nbyte) {
    if (real_write==NULL) {
        real_write = (ssize_t(*)(int,const void*,size_t))dlsym(RTLD_NEXT, "write");
        assert(real_write != NULL);
    }
    return real_write(fd, buf, nbyte);
}

static ssize_t (*real_read)(int fildes, const void *buf, size_t nbyte) = NULL;
ssize_t call_real_read(int fildes, const void *buf, size_t nbyte);
ssize_t call_real_read(int fildes, const void *buf, size_t nbyte) {
    if (real_read == NULL) {
        real_read = (ssize_t(*)(int,const void*,size_t))dlsym(RTLD_NEXT, "read");
        assert(real_read != NULL);
    }
    return real_read(fildes, buf, nbyte);
}

static ssize_t (*real_pwrite)(int fildes, const void *buf, size_t nbyte, off_t offset) = NULL;
ssize_t call_real_pwrite(int fildes, const void *buf, size_t nbyte, off_t offset);
ssize_t call_real_pwrite(int fildes, const void *buf, size_t nbyte, off_t offset) {
    if (real_pwrite == NULL) {
        real_pwrite = (ssize_t(*)(int,const void*,size_t,off_t))dlsym(RTLD_NEXT, "pwrite");
        assert(real_pwrite != NULL);
    }
    return real_pwrite(fildes, buf, nbyte, offset);
}

static int (*real_ftruncate)(int fildes,  off_t length) = NULL;
int call_real_ftruncate(int fildes, off_t length);
int call_real_ftruncate(int fildes, off_t length) {
    if (real_ftruncate == NULL) {
        real_ftruncate = (int(*)(int,off_t))dlsym(RTLD_NEXT, "ftruncate");
        assert(real_ftruncate);
    }
    return real_ftruncate(fildes, length);
}

static int (*real_truncate)(const char *path, off_t length) = NULL;
int call_real_truncate(const char *path, off_t length);
int call_real_truncate(const char *path, off_t length) {
    if (real_truncate == NULL) {
        real_truncate = (int(*)(const char *,off_t))dlsym(RTLD_NEXT, "truncate");
        assert(real_truncate);
    }
    return real_truncate(path, length);
}

static int (*real_unlink)(const char* path) = NULL;
int call_real_unlink(const char *path);
int call_real_unlink(const char *path) {
    if (real_unlink == NULL) {
        real_unlink = (int(*)(const char*))dlsym(RTLD_NEXT, "unlink");
        assert(real_unlink);
    }
    return real_unlink(path);
}

static int (*real_rename)(const char *oldpath, const char *newpath) = NULL;
int call_real_rename(const char* oldpath, const char* newpath);
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
