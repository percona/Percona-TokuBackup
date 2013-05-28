/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

class backtrace {
public:
    const char *file;
    const int line;
    const char *fun;
    const backtrace *prev;
    backtrace(const char *fi, int l, const char *fu, const backtrace *p) throw() : file(fi), line(l), fun(fu), prev(p) {}
};
