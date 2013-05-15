/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id: realpath_library.cc 55458 2013-04-13 21:44:50Z christianrober $"

#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <errno.h>

#include "realpath_library.h"

/***********************************************
 *
 *   This file interposes the realpath() system call.  We use it to
 *   test HotBackup's usage and error handling of realpath().
 *
 **********************************************/

namespace REALPATH {
    static bool INJECT_ERROR = false;
    void inject_error(bool b) { INJECT_ERROR = b; }
}

typedef char * (*realpath_function)(const char*, char*);

static realpath_function real_realpath = NULL;
char * call_real_path(const char *file_name, char *resolved_name)
{
    // There is more than one implementation of realpath in stdlib.
    // We have to use the latest version to be able to pass NULL as
    // the second argument and have realpath allocate a new string on
    // our behalf.
    real_realpath = (realpath_function)(dlvsym(RTLD_NEXT, "realpath", "GLIBC_2.3"));
    if (real_realpath == NULL) {
        printf("error...");
        printf("%s\n", dlerror());
        return NULL;
    }

    return real_realpath(file_name, resolved_name);
}

char * realpath(const char *file_name, char *resolved_name)
{
    if (REALPATH::INJECT_ERROR) {
        printf("Injecting Error\n");
        errno = REALPATH::ERROR_TO_INJECT;
        return NULL;
    }

    return call_real_path(file_name, resolved_name);
}
