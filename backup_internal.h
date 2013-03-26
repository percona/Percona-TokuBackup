/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#ifndef BACKUP_INTERNAL_H
#define BACKUP_INTERNAL_H

#include "backup.h"

#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id: backup.h 54696 2013-03-25 20:59:02Z bkuszmaul $"

extern "C" {
    unsigned long get_throttle(void);
}

void create_subdirectories(const char*);


#endif // end of header guardian.
