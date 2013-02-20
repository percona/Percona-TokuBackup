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
#include <unistd.h>

///////////////////////////////////////////////////////////////////////////////
//
// file_description() -
//
// Description: 
//
//     ...
//
file_description::file_description()
: m_refcount(1), m_offset(0), m_fd_in_dest_space(-1), m_name(NULL)
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
    printf("file_description: %s, @%p\n", m_name, this);
    printf("refcount:%d, offset:%ld, fd_in_dest_space:%d\n",
            this->m_refcount, this->m_offset, this->m_fd_in_dest_space);
    printf("======================\n");
}

///////////////////////////////////////////////////////////////////////////////
//
void file_description::set_name(char *name)
{
    m_name = name;
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
// Notes:
//
//     Open assumes that the backup file exists.  Create assumes the 
// backup file does NOT exist.
//
void file_description::open()
{
    int fd = 0;
    fd = call_real_open(m_name, O_WRONLY, 0777);
    if (fd < 0) {
        int error = errno;
        assert(error == ENOENT);
        this->create();
    } else {
        this->m_fd_in_dest_space = fd;
    }
}

///////////////////////////////////////////////////////////////////////////////
//
// create() -
//
// Description: 
//
//     Calls the operating system's open() syscall with the create
// flag for the current file description.  This also sets the file 
// descriptor in the destination/backup space for the backup
// copy of the original file.
//
// Notes:
//
//     Open assumes that the backup file exists.  Create assumes the 
// backup file does NOT exist.
//
void file_description::create()
{
    // Create file that was just opened, this assumes the parent directories
    // exist.
    int fd = 0;
    fd = call_real_open(m_name, O_CREAT | O_WRONLY, 0777);
    if (fd < 0) {
        int error = errno;
        perror("[UhOh]: <CAPTURE> ");
        assert(error == EEXIST);
        fd = call_real_open(m_name, O_WRONLY, 0777);
        if (fd < 0) {
            perror("BACKUP: Couldn't open backup copy of recently opened file.");
            abort();            
        }
    }

    this->m_fd_in_dest_space = fd;
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
    r = call_real_close(this->m_fd_in_dest_space);
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
    r = call_real_write(this->m_fd_in_dest_space, buf, nbyte);
    if (r < 0) {
        perror("BACKUP: write() to backup file failed."); 
        abort();
    }
        
    this->m_offset = r;
}

///////////////////////////////////////////////////////////////////////////////
//
// seek() -
//
// Description: 
//
//     ...
//
void file_description::seek(size_t nbyte)
{
    // Do we need to seek nbytes past the current position?
    // Past an absolute position?
    // <CER> this depends on the caller...
    int whence = SEEK_SET;
    off_t offset = 0;
    offset = lseek(this->m_fd_in_dest_space, nbyte, whence);
    if (offset < 0) {
        perror("BACKUP: lseek() failed.");
        abort();
    }
    this->m_offset = offset;
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
    r = call_real_pwrite(this->m_fd_in_dest_space, buf, nbyte, offset);
    if (r < 0) {
        perror("BACKUP: pwrite() to backup file failed."); 
        abort();
    }
}
