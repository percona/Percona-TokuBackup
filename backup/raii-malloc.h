/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:  
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include "stdlib.h"

template <class T> class with_object_to_free {
public:
    const T value;
    with_object_to_free(T v): value(v) {};
    ~with_object_to_free(void) { if(value) free((void*)value); }
};

template <class T> class with_malloced {
  public:
    const T value;
    with_malloced(size_t size): value((T)malloc(size)) {};
    ~with_malloced(void) { if(value) free((void*)value); }
};
