/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#ifndef BACKUP_MANAGER_H
#define BACKUP_MANAGER_H

#include "backup_directory.h"
#include "file_description.h"
#include <sys/types.h>
#include <vector>

class backup_manager
{
private:
    bool m_doing_backup;
    bool m_interpret_writes;
    backup_directory m_dir;
public:
    backup_manager();
    void start_backup();
    void stop_backup();
    void add_directory(const char *source_dir, const char *dest_dir);
    void create_file(const char *file);
    void open_file(const char *file, int oflag, int fd);
    void close_descriptor(int fd);
    void write_to_descriptor(int fd, const void *buf, size_t nbyte);
    void rename_file(const char *oldpath, const char *newpath);
};

#endif // End of header guardian.
