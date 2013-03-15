/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:


#include <stdio.h> // rename(),
#include <assert.h>
#include <fcntl.h> // open()
#include <unistd.h> // close(), write(), read(), unlink(), truncate(), etc.
#include <sys/stat.h> // mkdir()

//#include <sys/types.h>
#include <dlfcn.h>
#include <stdarg.h>

#include "backup.h"
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
int open(const char* file, int oflag, ...)
{
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
            assert(r==0);
            if (!S_ISFIFO(stats.st_mode)) {
                manager.open(fd, file, oflag);
            }
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
ssize_t write(int fd, const void *buf, size_t nbyte)
{
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
ssize_t read(int fd, void *buf, size_t nbyte)
{
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
ssize_t pwrite(int fd, const void *buf, size_t nbyte, off_t offset)
{
    ssize_t r = 0;
    TRACE("pwrite() intercepted, fd = ", fd);
    r = call_real_pwrite(fd, buf, nbyte, offset);
    manager.pwrite(fd, buf, nbyte, offset);
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
int ftruncate(int fd, off_t length)
{
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
int truncate(const char *path, off_t length)
{
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
int unlink(const char *path)
{
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
int rename(const char *oldpath, const char *newpath)
{
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
int mkdir(const char *pathname, mode_t mode)
{
    int r = 0;
    TRACE("mkidr() intercepted", pathname);
    r = call_real_mkdir(pathname, mode);
    manager.mkdir(pathname);
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
