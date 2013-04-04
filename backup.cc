/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include <stdio.h> // rename(),
#include <fcntl.h> // open()
#include <unistd.h> // close(), write(), read(), unlink(), truncate(), etc.
#include <sys/stat.h> // mkdir()

//#include <sys/types.h>
#include <errno.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <stdlib.h>

#include "backup_internal.h"
#include "backup_manager.h"
#include "real_syscalls.h"
#include "backup_debug.h"

#if DEBUG_HOTBACKUP
#define WARN(string, arg) HotBackup::InterposeWarn(string, arg);
#define TRACE(string, arg) HotBackup::InterposeTrace(string, arg);
#define ERROR(string, arg) HotBackup::InterposeError(string, arg);
#else
#define WARN(string,arg)
#define TRACE(string,arg)
#define ERROR(string,arg)
#endif

#if 0
// I had turned this on to see if we were running these destructors twice.  I don't see it, however.
class demo_twicer {
  public:
    demo_twicer(void);
    ~demo_twicer(void);
};
demo_twicer::demo_twicer(void) {
    printf("demo_twicer constructor.\n");
}
demo_twicer::~demo_twicer(void) {
    printf("demo_twicer destructor.\n");
}

demo_twicer twicer;
#endif

backup_manager manager;

//***************************************
//
// Interposed public API:
//
//***************************************

///////////////////////////////////////////////////////////////////////////////
//
// open() -
//
// Description: 
//
//     Either creates or opens a file in both the source directory
// and the backup directory.
//
extern "C" int open(const char* file, int oflag, ...) {
    int fd = 0;
    TRACE("open() intercepted, file = ", file);
    if (oflag & O_CREAT) {
        va_list ap;
        va_start(ap, oflag);
        mode_t mode = va_arg(ap, mode_t);
        va_end(ap);
        fd = call_real_open(file, oflag, mode);
        if (fd >= 0) { 
            manager.create(fd, file);
        }
    } else {
        fd = call_real_open(file, oflag);
        if (fd >= 0) {
            struct stat stats;
            int r = fstat(fd, &stats);
            if(r != 0) {
                goto out;
            }

            // TODO: What happens if we can't tell that the file is a FIFO?  Should we just the backup?  Skip this file?
            if (!S_ISFIFO(stats.st_mode)) {
                manager.open(fd, file, oflag);
            }
        }
    }

out:
    return fd;
}


///////////////////////////////////////////////////////////////////////////////
//
// close() -
//
// Description: 
//
//     Closes the file associated with the provided file descriptor
// in both the source and backup directories.
//
int close(int fd) {
    int r = 0;
    TRACE("close() intercepted, fd = ", fd);
    r = call_real_close(fd);
    manager.close(fd);
    return r;
}


///////////////////////////////////////////////////////////////////////////////
//
// write() -
//
// Description: 
//
//     Writes to the file associated with the given file descriptor
// in both the source and backup directories.
//
ssize_t write(int fd, const void *buf, size_t nbyte) {
    TRACE("write() intercepted, fd = ", fd);

    // Moved the write down into file_description->write, where a lock can be obtained

    return manager.write(fd, buf, nbyte);
}


///////////////////////////////////////////////////////////////////////////////
//
// read() -
//
// Description: 
//
//     Reads data into the given buffer from the source directory.
// 
// Note:
//
//     The backup manager needs to know that the offset for the 
// given file descriptor has changed, even though no bytes are
// read from the backup copy of the same file.
//
ssize_t read(int fd, void *buf, size_t nbyte) {
    TRACE("read() intercepted, fd = ", fd);
    // Moved the read down into file_description->read, where a lock can be obtained.
    
    return manager.read(fd, buf, nbyte);
}


///////////////////////////////////////////////////////////////////////////////
//
// pwrite() -
//
// Description: 
//
//     Writes to the file associated with the given file descriptor
// in both the source and backup directories.
//
ssize_t pwrite(int fd, const void *buf, size_t nbyte, off_t offset) {
    ssize_t r = 0;
    TRACE("pwrite() intercepted, fd = ", fd);
    r = call_real_pwrite(fd, buf, nbyte, offset);
    if (r>0) {
        // Don't try to write if there was an error in the application.
        manager.pwrite(fd, buf, r, offset);
    }
    return r;
}

off_t lseek(int fd, off_t offset, int whence) {
    TRACE("lseek() intercepted fd =", fd);
    return manager.lseek(fd, offset, whence);
}

///////////////////////////////////////////////////////////////////////////////
//
// ftruncate() -
//
// Description: 
//
//     Deletes a portion of the file based on the given file descriptor.
//
int ftruncate(int fd, off_t length) {
    int r = 0;
    TRACE("ftruncate() intercepted, fd = ", fd);
    r = call_real_ftruncate(fd, length);
    manager.ftruncate(fd, length);
    return r;
}


///////////////////////////////////////////////////////////////////////////////
//
// truncate() -
//
// Description: 
//
//     Deletes a portion of the given file based on the given length.
//
int truncate(const char *path, off_t length) throw() {
    int r = 0;
    TRACE("truncate() intercepted, path = ", path);
    r = call_real_truncate(path, length);
    manager.truncate(path, length);
    return r;
}


///////////////////////////////////////////////////////////////////////////////
//
// unlink() -
//
// Description: 
//
int unlink(const char *path) throw() {
    int r = 0;
    TRACE("unlink() intercepted, path = ", path);
    r = call_real_unlink(path);
    return r;
}


///////////////////////////////////////////////////////////////////////////////
//
// rename() -
//
// Description: 
//
int rename(const char *oldpath, const char *newpath) {
    int r = 0;
    TRACE("rename() intercepted","");
    TRACE("-> oldpath = ", oldpath);
    TRACE("-> newpath = ", newpath);
    r = call_real_rename(oldpath, newpath);
    manager.rename(oldpath, newpath);
    return r;
}

///////////////////////////////////////////////////////////////////////////////
//
// mkdir() -
//
// Description: 
//
int mkdir(const char *pathname, mode_t mode) throw() {
    int r = 0;
    TRACE("mkidr() intercepted", pathname);
    r = call_real_mkdir(pathname, mode);
    manager.mkdir(pathname);
    return r;
}

extern "C" int tokubackup_create_backup(const char *source_dirs[], const char *dest_dirs[], int dir_count,
                                        backup_poll_fun_t poll_fun, void *poll_extra,
                                        backup_error_fun_t error_fun, void *error_extra) {
    if (dir_count!=1) {
        error_fun(EINVAL, "Only one source directory may be specified for backup", error_extra);
        return EINVAL;
    }
    for (int i=0; i<dir_count; i++) {
        if (source_dirs[i]==NULL) {
            error_fun(EINVAL, "One of the source directories is NULL", error_extra);
            return EINVAL;
        }
        if (dest_dirs[i]==NULL) {
            error_fun(EINVAL, "One of the destination directories is NULL", error_extra);
            return EINVAL;
        }
        //int r = manager.add_directory(source_dirs[i], dest_dirs[i], poll_fun, poll_extra, error_fun, error_extra);
        //if (r!=0) return r;
    }

    backup_callbacks calls(poll_fun, poll_extra, error_fun, error_extra, &get_throttle);
    return manager.do_backup(source_dirs[0], dest_dirs[0], &calls);
}

extern "C" void tokubackup_throttle_backup(unsigned long bytes_per_second) {
    manager.set_throttle(bytes_per_second);
}

unsigned long get_throttle(void) {
    return manager.get_throttle();
}

char *malloc_snprintf(size_t size, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    char *result = (char*)malloc(size);
    vsnprintf(result, size, format, ap);
    va_end(ap);
    return result;
}

const char tokubackup_sql_suffix[] = "-E"; // Tim says that we want it to say -E on the end.

void backup_set_keep_capturing(bool b)
// Effect: see backup_internal.h
{
    manager.set_keep_capturing(b);
}
bool backup_is_capturing(void) {
    return manager.is_capturing();
}
void backup_set_start_copying(bool b)
{
    manager.set_start_copying(b);
}
