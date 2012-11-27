/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#include "backup.h"

#include <stdio.h> // rename(),
#include <assert.h>

#include <fcntl.h> // open()
#include <unistd.h> // close(), write(), read(), unlink(), truncate(), etc.


//#include <sys/types.h>
#include <dlfcn.h>
#include <stdarg.h>


#include "backup_manager.h"
#include "real_syscalls.h"
#include "backup_debug.h"

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
int open(const char* file, int oflag, ...)
{
    int fd = 0;
    if (DEBUG) printf("open called.\n");
    if (oflag & O_CREAT) {
        va_list ap;
        va_start(ap, oflag);
        mode_t mode = va_arg(ap, mode_t);
        va_end(ap);
        fd = call_real_open(file, oflag, mode);
        if (fd < 0) { 
            perror("Interposed open() w/ O_CREAT failed.");
        } else {
            manager.create(fd, file);
        }
    } else {
        fd = call_real_open(file, oflag);
        if (fd < 0) {
            perror("Interposed open() failed."); 
        } else {
            manager.open(fd, file, oflag);
        }
    }

    return fd;
}


///////////////////////////////////////////////////////////////////////////////
//
// open64() -
//
// Description: 
//
//     64-bit Linux version of open().
//
int open64(const char* file, int oflag, ...)
{
    int fd = 0;
    if (DEBUG) printf("open called.\n");
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
            manager.open(fd, file, oflag);
        }
    }

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
int close(int fd)
{
    int r = 0;
    if (DEBUG) printf("close called.\n");
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
ssize_t write(int fd, const void *buf, size_t nbyte)
{
    ssize_t r = 0;
    if (DEBUG) printf("write called.\n");
    r = call_real_write(fd, buf, nbyte);
    
    // <CER> this *SHOULD* seek in the backup copy as part of the
    // write() to the backup file...
    
    manager.write(fd, buf, nbyte);
    return r;
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
ssize_t read(int fd, void *buf, size_t nbyte)
{
    ssize_t r = 0;
    if (DEBUG) printf("read called.\n");
    r = call_real_read(fd, buf, nbyte);
    if (r >= 0) {
        manager.seek(fd, nbyte);
    }
    return r;
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
ssize_t pwrite(int fd, const void *buf, size_t nbyte, off_t offset)
{
    ssize_t r = 0;
    if (DEBUG) printf("pwrite called.\n");
    r = call_real_pwrite(fd, buf, nbyte, offset);
    manager.pwrite(fd, buf, nbyte, offset);
    return r;
}


///////////////////////////////////////////////////////////////////////////////
//
// ftruncate() -
//
// Description: 
//
//     Deletes a portion of the file based on the given file descriptor.
//
int ftruncate(int fd, off_t length)
{
    int r = 0;
    if (DEBUG) printf("ftruncate called.\n");
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
int truncate(const char *path, off_t length)
{
    int r = 0;
    if (DEBUG) printf("truncate called.\n");
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
int unlink(const char *path)
{
    int r = 0;
    if (DEBUG) printf("unlink called.\n");
    r = call_real_unlink(path);
    return r;
}


///////////////////////////////////////////////////////////////////////////////
//
// rename() -
//
// Description: 
//
int rename(const char *oldpath, const char *newpath)
{
    int r = 0;
    if (DEBUG) printf("rename called.\n");
    r = call_real_rename(oldpath, newpath);
    manager.rename(oldpath, newpath);
    return r;
}

/****************************************************************************/
//
// TODO: Separate the API for initiating, stopping and altering backup.
//
/****************************************************************************/

///////////////////////////////////////////////////////////////////////////////
//
// add_directory() -
//
// Description:
//
extern "C" void add_directory(const char* source, const char* destination)
{
    manager.add_directory(source, destination);
}


///////////////////////////////////////////////////////////////////////////////
//
// add_directory() -
//
// Description:
//
extern "C" void remove_directory(const char* source, const char* destination)
{
    manager.remove_directory(source, destination);
}


///////////////////////////////////////////////////////////////////////////////
//
// start_backup() -
//
// Description:
//
//     
//
extern "C" void start_backup(void)
{
    manager.start_backup();
}


///////////////////////////////////////////////////////////////////////////////
//
// stop_backup() -
//
// Description: 
//
extern "C" void stop_backup(void)
{
    manager.stop_backup();
}
