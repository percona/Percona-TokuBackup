/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#ifndef CHECK_H
#define CHECK_H

#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

void check_fun(long predicate, const char *expr, const char *file, int line) throw();

// Like assert, except it doesn't go away under NDEBUG.
// Do this with a function so that we don't get false answers on branch coverage.
#define check(x) check_fun((long)(x), #x, __FILE__, __LINE__)

#endif
