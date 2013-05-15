/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id: realpath_library.h 55458 2013-04-13 21:44:50Z christianrober $"

#ifndef REALPATH_LIBRARY_H
#define REALPATH_LIBRARY_H

namespace REALPATH {
    const int ERROR_TO_INJECT = ENOMEM;
    void inject_error(bool);
}

#endif // End of header guardian.
