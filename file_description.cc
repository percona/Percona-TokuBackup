/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#include "file_description.h"
#include "real_syscalls.h"
#include "backup_debug.h"

#include "assert.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

///////////////////////////////////////////////////////////////////////////////
//
// file_description() -
//
// Description: 
//
//     ...
//
file_description::file_description()
: refcount(1), offset(0), fd_in_dest_space(-1), name(0)
{}

///////////////////////////////////////////////////////////////////////////////
//
// print() -
//
// Description: 
//
//     Usefull print out of file description state.
//
void file_description::print() {
    printf("======================\n");
    printf("file_description: %s, @%p\n", name, this);
    printf("refcount:%d, offset:%lld, fd_in_dest_space:%d\n",
            this->refcount, this->offset, this->fd_in_dest_space);
    printf("======================\n");
}

///////////////////////////////////////////////////////////////////////////////
//
// open() -
//
// Description: 
//
//     Calls the operating system's open() syscall for the current
// file description.  This also sets the file descriptor in the 
// destination/backup space for the backup copy of the original file.
//
void file_description::open()
{
    // Create file that was just opened, this assumes the parent directories
    // exist.
    int fd = 0;
    fd = call_real_open(this->name, O_CREAT | O_WRONLY, 0777);
    if (fd < 0) {
        int error = errno;
        perror("BACKUP: Couldn't create backup copy of recently opened file.");
        assert(error == EEXIST);
    }
    
    this->fd_in_dest_space = fd;
}

///////////////////////////////////////////////////////////////////////////////
//
// close() -
//
// Description: 
//
//     ...
//
void file_description::close()
{
    // TODO: Check refcount, if it's zero we REALLY have to close
    // the file.  Otherwise, if there are any references left, 
    // we can only decrement the refcount; other file descriptors
    // are still open in the main application.
    
    int r = 0;
    r = call_real_close(this->fd_in_dest_space);
    if (r != 0) {
        perror("BACKUP: close() of backup file failed."); 
        abort();
    }
}

///////////////////////////////////////////////////////////////////////////////
//
// write() -
//
// Description: 
//
//     ...
//
void file_description::write(const void *buf, size_t nbyte)
{
    ssize_t r = 0;
    r = call_real_write(this->fd_in_dest_space, buf, nbyte);
    if (r < 0) {
        perror("BACKUP: write() to backup file failed."); 
        abort();
    }
        
    // TODO: Update this file description's offset with amount that
    // was written.
    // ...
}

void file_description::seek(size_t nbyte __attribute__((__unused__))) {
    
}

///////////////////////////////////////////////////////////////////////////////
//
// pwrite() -
//
// Description: 
//
//     ...
//
void file_description::pwrite(const void *buf, size_t nbyte, off_t offset)
{
    ssize_t r = 0;
    r = call_real_pwrite(this->fd_in_dest_space, buf, nbyte, offset);
    if (r < 0) {
        perror("BACKUP: pwrite() to backup file failed."); 
        abort();
    }
    
    // TODO: Update this file description's offset
}
