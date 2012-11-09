/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#ifndef BACKUP_H
#define BACKUP_H

#include <sys/types.h>

extern "C" {
    //int open(const char *file, int oflag, ...);
    //int close(int fd);
    //ssize_t write(int fd, const void *buf, size_t nbyte);
//ssize_t pread(int d, void *buf, size_t nbyte, off_t offset);
//ssize_t read(int fildes, void *buf, size_t nbyte);
//ssize_t pwrite(int fildes, const void *buf, size_t nbyte, off_t offset);
//    int ftruncate(int fildes, off_t length) throw();
//    int truncate(const char *path, off_t length) throw();
    //   int unlink(const char *path) throw();
    //    int rename (const char *oldpath, const char *newpath) throw();
}

void start_backup(const char*, const char*);
void stop_backup();

#endif // end of header guardian.
