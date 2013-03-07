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
file_description::file_description(void)
: m_refcount(1), 
m_offset(0), 
m_fd_in_dest_space(DEST_FD_INIT), 
m_backup_name(NULL),
m_full_source_name(NULL), 
m_in_source_dir(false)
{
    int r = pthread_mutex_init(&m_mutex, NULL);
    assert(r==0);
}

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
void file_description::open(void)
{
    int fd = 0;
    fd = call_real_open(m_backup_name, O_WRONLY, 0777);
    if (fd < 0) {
        int error = errno;

        // For now, don't store the fd if they are opening a dir.
        // That is just for fsync'ing a dir, which we do not care about.
        if(error == EISDIR) {
            return;
        }

        if(error != ENOENT && error != EISDIR) {
            perror("ERROR: <CAPTURE> ");
            abort();
        }

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
void file_description::create(void)
{
    // Create file that was just opened, this assumes the parent directories
    // exist.
    int fd = 0;
    fd = call_real_open(m_backup_name, O_CREAT | O_WRONLY, 0777);
    if (fd < 0) {
        int error = errno;
        assert(error == EEXIST);
        fd = call_real_open(m_backup_name, O_WRONLY, 0777);
        if (fd < 0) {
            perror("ERROR: <CAPTURE>: Couldn't open backup copy of recently opened file.");
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
void file_description::close(void)
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
ssize_t file_description::write(int fd_in_source, const void *buf, size_t nbyte)
{
    pthread_mutex_lock(&m_mutex);
    ssize_t r = call_real_write(fd_in_source, buf, nbyte);
    if (r>0) {
        off_t position = m_offset;
        m_offset += r;
    
        if (!m_in_source_dir) {
            /* nothing */
        } else if (m_fd_in_dest_space == DEST_FD_INIT) {
            // We can't write to the backup file if it hasn't been created yet.
            /* nothing */
        } else {
            ssize_t second_write_size = call_real_pwrite(this->m_fd_in_dest_space, buf, r, position);
            assert(second_write_size==r);
        }
    }
    pthread_mutex_unlock(&m_mutex);
    return r;
}

ssize_t file_description::read(int fd_in_source, void *buf, size_t nbyte) {
    pthread_mutex_lock(&m_mutex);
    ssize_t r = call_real_read(fd_in_source, buf, nbyte);
    m_offset += r;
    pthread_mutex_unlock(&m_mutex);
    return r;
}


///////////////////////////////////////////////////////////////////////////////
//
// seek() -
//
// Description: 
//
//     ...
//
off_t file_description::lseek(int fd_in_source, size_t nbyte, int whence) {
    pthread_mutex_lock(&m_mutex);
    off_t new_offset = call_real_lseek(fd_in_source, nbyte, whence);
    this->m_offset = new_offset;
    pthread_mutex_unlock(&m_mutex);
    return new_offset;
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

///////////////////////////////////////////////////////////////////////////////
//
// truncate() -
//
// Description: 
//
//     ...
//
void file_description::truncate(off_t length)
{
    if(!m_in_source_dir) {
        return;
    }

    if (m_fd_in_dest_space == DEST_FD_INIT) {
        return;
    }

    int r = 0;
    r = call_real_ftruncate(this->m_fd_in_dest_space, length);
    if (r < 0) {
        perror("ERROR: <CAPTURE>: ");
        abort();
    }
}
