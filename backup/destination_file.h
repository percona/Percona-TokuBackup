/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id: destination_file.h 55908 2013-04-26 09:58:08Z bkuszmaul $"

#ifndef DESTINATION_FILE_H
#define DESTINATION_FILE_H

#include <sys/types.h>

class destination_file {
public:
    destination_file(const int opened_fd, const char * full_path) throw();
    ~destination_file() throw();
    int close(void) const throw();
    int pwrite(const void *buf, size_t nbyte, off_t offset) const throw();
    int truncate(off_t length) const throw();
    int unlink(void) const throw();
    int rename(const char *new_path) throw();
    int get_fd(void) const throw();
    const char * get_path(void) const throw();
private:
    const int m_fd;
    const char * m_path;
};

#endif // End of header guardian.
