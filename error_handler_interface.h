/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id: error_handler_interface.h 55281 2013-04-09 21:09:36Z christianrober $"

#ifndef ERROR_HANDLER_INTERFACE_H
#define ERROR_HANDLER_INTERFACE_H

class error_handler_interface {
public:
    virtual ~error_handler_interface() {};
    virtual void fatal_error(int errnum, const char *format, ...) = 0;
    virtual void backup_error(int errnum, const char *format, ...) = 0;
};

#endif // End of header guardian.
