/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#include "file_description.h"
#include "real_syscalls.h"
#include "backup_debug.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

const int DEST_FD_INIT = -1;

///////////////////////////////////////////////////////////////////////////////
//
// file_description() -
//
// Description: 
//
//     ...
//
file_description::file_description()
: m_refcount(1), 
m_offset(0), 
m_fd_in_dest_space(DEST_FD_INIT), 
m_backup_name(NULL),
m_full_source_name(NULL), 
m_in_source_dir(false)
{}

///////////////////////////////////////////////////////////////////////////////
//
void file_description::prepare_for_backup(const char *name)
{
    // TODO: strdup this string, then free it later.
    m_backup_name = strdup(name);
    m_in_source_dir = true;
}

///////////////////////////////////////////////////////////////////////////////
//
void file_description::set_full_source_name(const char *name)
{
    // TODO: strdup this string, then free it later.
    m_full_source_name = strdup(name);
}

///////////////////////////////////////////////////////////////////////////////
//
const char * file_description::get_full_source_name(void)
{
    return m_full_source_name;
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
    fd = call_real_open(m_backup_name, O_WRONLY, 0777);
    if (fd < 0) {
        int error = errno;
        if(error != ENOENT) {
            perror("ERROR: <CAPTURE> ");
        }
        
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
    fd = call_real_open(m_backup_name, O_CREAT | O_WRONLY, 0777);
    if (fd < 0) {
        int error = errno;
        if (error != EEXIST) {
            perror("ERROR: <CAPTURE> ");
        }

        assert(error == EEXIST);
        fd = call_real_open(m_backup_name, O_WRONLY, 0777);
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
    if(!m_in_source_dir) {
        return;
    }
    
    if(m_fd_in_dest_space == DEST_FD_INIT) {
        return;
    }

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
    this->m_offset += nbyte;
    
    if(!m_in_source_dir) {
        return;
    }
    
    // We can't write to the backup file if it hasn't been created yet.
    if(m_fd_in_dest_space == DEST_FD_INIT) {
        return;
    }

    ssize_t r = 0;
    size_t position = m_offset - nbyte;
    r = call_real_pwrite(this->m_fd_in_dest_space, buf, nbyte, position);
    if (r < 0) {
        perror("BACKUP: write() to backup file failed."); 
        abort();
    }
}

///////////////////////////////////////////////////////////////////////////////
//
// seek() -
//
// Description: 
//
//     ...
//
void file_description::seek(size_t nbyte, int whence)
{
    switch(whence) {
        case SEEK_SET:
            this->m_offset = nbyte;
            break;
        case SEEK_CUR:
            this->m_offset += nbyte;
            break;
        case SEEK_END:
            // TODO: How do we know how big the source file is?
            // Do we need to track this as well.
            break;
        default:
            break;
    }
    
    if(!m_in_source_dir) {
        return;
    }
    
    // We can't seek the backup file if it hasn't been created yet.
    if(m_fd_in_dest_space == DEST_FD_INIT) {
        return;
    }
    
    // Do we need to seek nbytes past the current position?
    // Past an absolute position?
    // <CER> this depends on the caller...
    off_t offset = 0;
    offset = lseek(this->m_fd_in_dest_space, nbyte, whence);
    if (offset < 0) {
        perror("BACKUP: lseek() failed.");
        abort();
    }
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
    if(!m_in_source_dir) {
        return;
    }
    
    if(m_fd_in_dest_space == DEST_FD_INIT) {
        return;
    }
    
    ssize_t r = 0;
    r = call_real_pwrite(this->m_fd_in_dest_space, buf, nbyte, offset);
    if (r < 0) {
        perror("BACKUP: pwrite() to backup file failed."); 
        abort();
    }
}
