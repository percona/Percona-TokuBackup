/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
//#include <sys/types.h>
#include <dlfcn.h>
#include <stdarg.h>

#include "backup.h"
#include "backup_manager.h"
#include "real_syscalls.h"

backup_manager manager;

//
// Interposed public API:
//
int open(const char* file, int oflag, ...)
{
    int r = 0;
    printf("open called.\n");
    if (oflag & O_CREAT) {
        va_list ap;
        va_start(ap, oflag);
        mode_t mode = va_arg(ap, mode_t);
        va_end(ap);
        r = call_real_open(file, oflag, mode);
        manager.create_file(file);
    } else {
        r = call_real_open(file, oflag);
        manager.open_file(file, oflag, r);
    }

    return r;
}

int close(int fd)
{
    int r = 0;
    printf("close called.\n");
    r = call_real_close(fd);
    manager.close_descriptor(fd);
    return r;
}

ssize_t write(int fd, const void *buf, size_t nbyte)
{
    ssize_t r = 0;
    printf("write called.\n");
    r = call_real_write(fd, buf, nbyte);
    manager.write_to_descriptor(fd, buf, nbyte);
    return r;
}

int rename(const char *oldpath, const char *newpath)
{
    int r = 0;
    printf("rename called.\n");
    r = call_real_rename(oldpath, newpath);
    manager.rename_file(oldpath, newpath);
    return r;
}

// TODO: Separate the API for initiating, stopping and altering backup.
void start_backup(const char *source_dir_arg, const char *dest_dir_arg)
{
    manager.start_backup();
    manager.add_directory(source_dir_arg, dest_dir_arg);
}
