/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#ifndef BACKUP_CALLBACKS_H
#define BACKUP_CALLBACKS_H

#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include "backup_internal.h"

typedef unsigned long (*backup_throttle_fun_t)(void);

//////////////////////////////////////////////////////////////////////////////
//
class backup_callbacks
{
public:
    backup_callbacks(backup_poll_fun_t poll_fun, 
                     void *poll_extra, 
                     backup_error_fun_t error_fun, 
                     void *error_extra,
                     backup_throttle_fun_t throttle_fun) throw();
    int poll(float progress, const char *progress_string) throw();
    void report_error(int error_number, const char *error_description) throw();
    unsigned long get_throttle(void) throw();
private:
    backup_callbacks() throw() {};
    backup_poll_fun_t m_poll_function;
    void *m_poll_extra;
    backup_error_fun_t m_error_function;
    void *m_error_extra;
    backup_throttle_fun_t m_throttle_function;
};

#endif // end of header guardian.
