/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:  
#include "backup_manager.h"
#include "real_syscalls.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#define DEBUG 1

// Helper print function.
static void print(file_description *desc) {
    printf("file_description %p:\n", desc);
    printf("refcount %d:\n", desc->refcount);
    printf("offset %d:\n", desc->offset);
    printf("fd_in_dest_space %d:\n", desc->fd_in_dest_space);
}

//
backup_manager::backup_manager() 
    : m_doing_backup(false),
      m_interpret_writes(false)
{}

void backup_manager::start_backup()
{
    assert(m_doing_backup == false);
    m_doing_backup = true;
    m_interpret_writes = true;
}

void backup_manager::stop_backup()
{
    assert(m_doing_backup == true);
    m_doing_backup = false;
    m_interpret_writes = false;
}

void backup_manager::add_directory(const char *source_dir,
                                   const char *dest_dir)
{
    // TEMP: We only have one directory at this point.
    m_dir.set_directories(source_dir, dest_dir);
    // Start backing all the files in the directory.
    m_dir.start_copy();
}

void backup_manager::create_file(const char *file) 
{
    // TODO: Implement this.
}

void backup_manager::open_file(const char *file, int oflag, int fd)
{
    // 1. Verify that the prefix is correct.
    if (!m_dir.is_prefix(file)) {
        return;
    }

    // 2. Create new file name based on master file prefix.
    char *new_name = m_dir.translate_prefix(file);
    if (DEBUG) printf("new_name = %s\n", new_name);

    // 3. Allocate file description object.
    // TODO: use 'new', since that will call the constructor and initialize
    // our object via the intitialization list.
    file_description *description = 0;
    description = (file_description*)malloc(sizeof(file_description));
    description->refcount = 1;
    description->offset   = 0;
    //    description->fds.init();
    description->fds.push_back(0); // fd?

    // 4. actually open the new file in the backup dir.
    // TODO: Do we ever want to use a different oflag for the backup?
    // Maybe exclusive?
    printf(new_name);
    int r = 0;
    r  = call_real_open(new_name, oflag);
    if (r < 0) perror("open");
    description->fd_in_dest_space = r;
    if (DEBUG) print(description);

    // 5. Add description to array.
    m_dir.add_description(fd, description);
}

void backup_manager::close_descriptor(int fd) 
{

}

void backup_manager::write_to_descriptor(int fd, const void *buf, size_t nbyte)
{
    if (!m_interpret_writes) {
        return;
    }

    file_description *fdesc = 0;
    fdesc = m_dir.get_file_description(fd);
    // TEMP:
    assert(fdesc != 0);
    ssize_t r = call_real_write(fdesc->fd_in_dest_space, buf, nbyte);
    assert(r == nbyte);
    // TODO: Update this file description's offset with amount that
    // was written.
}

void backup_manager::rename_file(const char *oldpath, const char *newpath) 
{
    // TODO:
}

