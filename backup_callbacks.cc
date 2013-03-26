/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id: backup_directory.cc 54695 2013-03-25 20:57:54Z bkuszmaul $"

#include "backup_callbacks.h"

//////////////////////////////////////////////////////////////////////////////
//
backup_callbacks::backup_callbacks(backup_poll_fun_t poll_fun, 
                                   void *poll_extra, 
                                   backup_error_fun_t error_fun, 
                                   void *error_extra,
                                   backup_throttle_fun_t throttle_fun)
: m_poll_function(poll_fun), 
m_poll_extra(poll_extra), 
m_error_function(error_fun), 
m_error_extra(error_extra),
m_throttle_function(throttle_fun)
{}

//////////////////////////////////////////////////////////////////////////////
//
int backup_callbacks::poll(float progress, const char *progress_string)
{
    int r = 0;
    r = m_poll_function(progress, progress_string, m_poll_extra);
    return r;
}

//////////////////////////////////////////////////////////////////////////////
//
void backup_callbacks::report_error(int error_number, const char *error_str)
{
    m_error_function(error_number, error_str, m_error_extra);
}

//////////////////////////////////////////////////////////////////////////////
//
unsigned long backup_callbacks::get_throttle(void)
{
    return m_throttle_function();
}

