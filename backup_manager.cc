/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */

// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:  
#include "backup_manager.h"
#include "real_syscalls.h"
#include "backup_debug.h"

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>

///////////////////////////////////////////////////////////////////////////////
//
// backup_manager() -
//
// Description: 
//
//     Constructor.
//
backup_manager::backup_manager() 
    : m_doing_backup(false),
      m_interpret_writes(false)
{
}


///////////////////////////////////////////////////////////////////////////////
//
// start_backup() -
//
// Description: 
//
//     Turns ON boolean flags for both backup and the interpretation
// of writes.
//
void backup_manager::start_backup()
{
    assert(m_doing_backup == false);
    int r = pthread_mutex_init(&m_mutex, NULL);
    if (r) { perror("backup mutex creation failed."); abort(); }
    
    m_doing_backup = true;
    
    // TODO: For now, don't interpret writes...
    m_interpret_writes = false;
    
    // Start backing all the files in the directory.
    m_dir.start_copy();
}


///////////////////////////////////////////////////////////////////////////////
//
// stop_backup() -
//
// Description: 
//
//     Turns OFF boolean flags for both backup and the interpretation
// of writes.
//
void backup_manager::stop_backup()
{
    assert(m_doing_backup == true);
    
    // TODO: Make this: abort copy?
    m_dir.wait_for_copy_to_finish();
    
    m_doing_backup = false;
    
    int r = pthread_mutex_destroy(&m_mutex);
    if (r) { perror("Cannot destroy backup mutex."); abort(); }
}


///////////////////////////////////////////////////////////////////////////////
//
// add_directory() -
//
// Description: 
//
//     Adds the given source directory to our set of directories
// to backup.  This also uses the given destination argument as
// the top level of our backup directory.  All files underneath
// each directory tree should match once the backup is complete.
//
void backup_manager::add_directory(const char *source_dir,
                                   const char *dest_dir)
{
    assert(source_dir);
    assert(dest_dir);
    
    // TODO: assert that the directory's are not the same.
    // TODO: assert that the destination directory is empty.
    
    // TEMP: We only have one directory object at this point, for now...
    m_dir.set_directories(source_dir, dest_dir);
}

///////////////////////////////////////////////////////////////////////////////
//
// remove_directory() -
//
// Description: 
//
//     TBD...
//
void backup_manager::remove_directory(const char *source_dir,
                                      const char *dest_dir)
{
    assert(source_dir);
    assert(dest_dir);
    
    // TODO: Do we need to pause copy?
    // TODO: Do we want an option to erase the backup?
}


///////////////////////////////////////////////////////////////////////////////
//
// create_file() -
//
// Description: ???.
//
void backup_manager::create(int fd, const char *file) 
{
    backup_directory *directory = this->get_directory(file);
    if (directory == NULL) {
        return;
    }
    
    directory->create(fd, file);

    // TODO: Add new file name to file_description object. 
    m_map.put(fd);
    file_description *description = m_map.get(fd);
    description->name = directory->translate_prefix(file);
    directory->open(description);
    description->open();
}


///////////////////////////////////////////////////////////////////////////////
//
// open_file() -
//
// Description: 
//
//     If the given file is in our source directory, this method
// creates a new file_description object and opens the file in
// the backup directory.  We need the bakcup copy open because
// it may be updated if and when the user updates the original/source
// copy of the file.
//
void backup_manager::open(int fd, const char *file, int oflag)
{
    if (MGR_DBG) printf("entering open() with fd = %d\n", fd);
    
    backup_directory *directory = this->get_directory(file);
    if (directory == NULL) {
        return;
    }

    m_map.put(fd);
    file_description * const description = m_map.get(fd);
    description->name = directory->translate_prefix(file);
    
    directory->open(description);
    description->open();
}


///////////////////////////////////////////////////////////////////////////////
//
// close_descriptor() -
//
// Description:
//
//     Find and deallocate file description based on incoming fd.
//
void backup_manager::close(int fd) 
{
    if (MGR_DBG) printf("entering close_descriptor() with fd = %d\n", fd);
    file_description *description = m_map.get(fd);
    if (description == NULL) {
        printf("Cannot close the given description: doesn't exist in map.\n");
        return;
    }    

    description->close();
    
    // TODO: ??? 
    // What happens when we have multiple handles on one file descriptor,
    // but we close only one of them?...
    m_map.erase(fd);
}


///////////////////////////////////////////////////////////////////////////////
//
// write_to_descriptor() -
//
// Description: 
//
//     Using the given file descriptor, this method updates the 
// backup copy of a prevously opened file.
//
void backup_manager::write(int fd, const void *buf, size_t nbyte)
{
    if (MGR_DBG) printf("entering backup write() with fd = %d\n", fd);
    if (!m_interpret_writes) {
        return;
    }

    file_description *description = NULL;
    description = m_map.get(fd);
    if (description == NULL) {
        return;
    }
    
    description->write(buf, nbyte);
}


///////////////////////////////////////////////////////////////////////////////
//
// write_to_descriptor() -
//
// Description: 
//
void backup_manager::pwrite(int fd, const void *buf, size_t nbyte, off_t offset)
{
    if (MGR_DBG) printf("entering backup pwriter() with fd = %d\n", fd);
    if (!m_interpret_writes) {
        return;
    }
    
    file_description *description = NULL;
    description = m_map.get(fd);
    if (description == NULL) {
        return;
    }
    
    description->pwrite(buf, nbyte, offset);
}


///////////////////////////////////////////////////////////////////////////////
//
// seek() -
//
// Description: 
//
//     Move the backup file descriptor to the new position.  This allows
// upcoming intercepted writes to be backed up properly.
//
void backup_manager::seek(int fd, size_t nbyte)
{
    if (MGR_DBG) printf("entering seek(), with fd = %d\n", fd);
    if (!m_interpret_writes) {
        return;
    }
    
    file_description *description = NULL;
    description = m_map.get(fd);
    if (description == NULL) {
        return;
    }
    
    // TODO: determine who is seeking...
    // Do we need to seek nbytes past the current position?
    // Past an absolute position?
    // <CER> this depends on the caller...
    int whence = SEEK_SET;
    lseek(description->fd_in_dest_space, nbyte, whence);
}


///////////////////////////////////////////////////////////////////////////////
//
// rename_file() -
//
// Description: ???.
//
void backup_manager::rename(const char *oldpath, const char *newpath)
{
    // TODO:
}


///////////////////////////////////////////////////////////////////////////////
//
// () -
//
// Description: 
//
//     ...
//
backup_directory* backup_manager::get_directory(int fd)
{
    return &m_dir;
}

///////////////////////////////////////////////////////////////////////////////
//
// () -
//
// Description: 
//
//     ...
//
backup_directory* backup_manager::get_directory(const char *file)
{
    if (MGR_DBG) printf("entering create_file() with file name = %s\n", file);
    
    if (!m_dir.directories_set())
    {
        return NULL;
    }
    
    // See if file is in backup directory or not...
    if (!m_dir.is_prefix(file)) {
        return NULL;
    }
    
    return &m_dir;
}
