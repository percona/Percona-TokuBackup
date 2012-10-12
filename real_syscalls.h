/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#ifndef REAL_SYSCALLS_H
#define REAL_SYSCALLS_H

#include <sys/types.h>

int call_real_open(const char *file, int oflag, ...);
int call_real_close(int fd);
ssize_t call_real_write(int fd, const void *buf, size_t nbyte);
int call_real_rename(const char* oldpath, const char* newpath);

#endif // end of header guardian.
