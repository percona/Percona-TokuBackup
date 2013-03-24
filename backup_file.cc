/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include "backup_file.h"

#include <string.h>
#include <stdlib.h>

backup_file::backup_file()
    : file(0),
      fd(0),
      oflag(0)
{}

void backup_file::set_file(int fd, const char *file, int oflag)
{
    this->file = file;
    this->fd = fd;
    this->oflag = oflag;
}
