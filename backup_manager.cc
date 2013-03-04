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

#if DEBUG_HOTBACKUP
#define WARN(string, arg) HotBackup::CaptureWarn(string, arg)
#define TRACE(string, arg) HotBackup::CaptureTrace(string, arg)
#define ERROR(string, arg) HotBackup::CaptureError(string, arg)
#else
#define WARN(string, arg) 
#define TRACE(string, arg) 
#define ERROR(string, arg) 
#endif

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
      m_doing_copy(true) // <CER> Set to false to turn off copy
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

    m_doing_backup = true;
    
    // Start backing all the files in the directory.
    if (m_doing_copy) {
        m_dir.start_copy();
    }
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
    
    // Loop through all the current file descriptions and prepare them
    // for backup.
    for(int i = 0; i < m_map.size(); ++i) {
        file_description *file = m_map.get(i);
        if(file == NULL) {
            continue;
        }
        
        const char * source_path = file->get_full_source_name();
        if(!m_dir.is_prefix(source_path)) {
            continue;
        }

        const char * file_name = m_dir.translate_prefix(source_path);
        file->prepare_for_backup(file_name);
        m_dir.open_path(file_name);
        file->create();
    }
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
// create() -
//
// Description:
//
//     TBD: How is create different from open?  Is the only
// difference that we KNOW the file doesn't yet exist (from
// copy) for the create case?
//
void backup_manager::create(int fd, const char *file) 
{
    TRACE("entering create() with fd = ", fd);
    m_map.put(fd);
    file_description *description = m_map.get(fd);
    description->set_full_source_name(file);
    
    // If this file is not in the path of our source dir, don't create it.
    backup_directory *directory = this->get_directory(file);
    if (directory == NULL) {
        return;
    }

    char *backup_file_name = directory->translate_prefix(file);
    directory->open_path(backup_file_name);
    description->prepare_for_backup(backup_file_name);
    description->create();
}


///////////////////////////////////////////////////////////////////////////////
//
// open() -
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
    TRACE("entering open() with fd = ", fd);
    m_map.put(fd);
    file_description *description = m_map.get(fd);
    description->set_full_source_name(file);

    // If this file is not in the path of our source dir, don't open it.
    backup_directory *directory = this->get_directory(file);
    if (directory == NULL) {
        return;
    }

    char *backup_file_name = directory->translate_prefix(file);
    directory->open_path(backup_file_name);
    description->prepare_for_backup(backup_file_name);
    description->open();

    oflag++;
}


///////////////////////////////////////////////////////////////////////////////
//
// close() -
//
// Description:
//
//     Find and deallocate file description based on incoming fd.
//
void backup_manager::close(int fd) 
{
    TRACE("entering close() with fd = ", fd);
    m_map.erase(fd); // If the fd exists in the map, close it and remove it from the mmap.
}


///////////////////////////////////////////////////////////////////////////////
//
// write() -
//
// Description: 
//
//     Using the given file descriptor, this method updates the 
// backup copy of a prevously opened file.
//     Also does the write itself (the write is in here so that a lock can be obtained to protect the file offset)
//
ssize_t backup_manager::write(int fd, const void *buf, size_t nbyte)
{
    TRACE("entering write() with fd = ", fd);

    file_description *description = m_map.get(fd);
    if (description == NULL) {
        return call_real_write(fd, buf, nbyte);
    } else {
        return description->write(fd, buf, nbyte);
    }
}

// read(): Do the read.
ssize_t backup_manager::read(int fd, void *buf, size_t nbyte) {
    TRACE("entering write() with fd = ", fd);
    file_description *description = m_map.get(fd);
    if (description == NULL) {
        return call_real_read(fd, buf, nbyte);
    } else {
        return description->read(fd, buf, nbyte);
    }
}

///////////////////////////////////////////////////////////////////////////////
//
// pwrite() -
//
// Description:
//
//     Same as regular write, but uses additional offset argument
// to write to a particular position in the backup file.
//
void backup_manager::pwrite(int fd, const void *buf, size_t nbyte, off_t offset)
{
    TRACE("entering pwrite() with fd = ", fd);

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
void backup_manager::seek(int fd, size_t nbyte, int whence)
{
    TRACE("entering seek() with fd = ", fd);
    file_description *description = NULL;
    description = m_map.get(fd);
    if (description == NULL) {
        return;
    }

    description->seek(nbyte, whence);
}


///////////////////////////////////////////////////////////////////////////////
//
// rename() -
//
// Description:
//
//     TBD...
//
void backup_manager::rename(const char *oldpath, const char *newpath)
{
    TRACE("entering rename()...", "");
    TRACE("-> old path = ", oldpath);
    TRACE("-> new path = ", newpath);
    // TODO:
    oldpath++;
    newpath++;
}

///////////////////////////////////////////////////////////////////////////////
//
// ftruncate() -
//
// Description:
//
//     TBD...
//
void backup_manager::ftruncate(int fd, off_t length)
{
    TRACE("entering ftruncate with fd = ", fd);
    // TODO:
    fd++;
    length++;
}

///////////////////////////////////////////////////////////////////////////////
//
// truncate() -
//
// Description:
//
//     TBD...
//
void backup_manager::truncate(const char *path, off_t length)
{
    TRACE("entering truncate() with path = ", path);
    // TODO:
    if(path) {
        length++;
    }
}

///////////////////////////////////////////////////////////////////////////////
//
// mkdir() -
//
// Description:
//
//     TBD...
//
void backup_manager::mkdir(const char *pathname)
{
    backup_directory *directory = this->get_directory(pathname);
    if (directory == NULL) {
        return;
    }

    TRACE("entering mkdir() for:", pathname); 
    char *backup_directory_name = directory->translate_prefix(pathname);
    directory->open_path(backup_directory_name);
}

///////////////////////////////////////////////////////////////////////////////
//
// get_directory() -
//
// Description: 
//
//     TODO: When there are multiple directories, this method
// will use the given file descriptor to return the backup
// directory associated with that file descriptor.
//
backup_directory* backup_manager::get_directory(int fd)
{
    fd++;
    return &m_dir;
}


///////////////////////////////////////////////////////////////////////////////
//
// get_directory() -
//
// Description: 
//
//     TODO: Needs to change for multiple directories.
//
backup_directory* backup_manager::get_directory(const char *file)
{
    //TODO: Add relevant tracing.
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
