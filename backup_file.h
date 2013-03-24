/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include "file_description.h"

class backup_file {
private:
    const char *file;
    int fd;
    int oflag;
public:
    backup_file();
    void set_file(int fd, const char *file, int oflag);
};
