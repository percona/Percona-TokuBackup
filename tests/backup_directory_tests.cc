/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

#include "backup_test_helpers.h"
#include "backup_internal.h"
#include "backup_directory.h"

#define LONG_DIR "/ThisIsALongDirectory/WithNothing/InIt/"

static int backup_sub_dirs(void) {
    setup_destination();
    setup_source();

    char *src = get_src();
    char *dst = get_dst();
    char *source = realpath(src, NULL);
    char *destination = realpath(dst, NULL);

    // We need to pretend that there is a source directory with this
    // long path.  So, let's first create enough space for it.
    char *newpath = (char*)malloc(PATH_MAX);
    strcpy(newpath, destination);
    char *temp = newpath;

    // Move to the end of the absolute destination path we just
    // copied.
    while(*temp++) {;}
    --temp;

    // Append the long directory path to the destination path we just
    // copied.
    const char *longname = LONG_DIR;
    while(*longname) {
        *temp = *(longname)++;
        temp++;
    }

    *temp = 0;
    {
        int r = create_subdirectories(newpath);
        assert(r==0);
    }
    free(newpath);

    // Verify:
    struct stat sb;
    char dst_long_dir[PATH_MAX];
    {
        int r = snprintf(dst_long_dir, sizeof(dst_long_dir), "%s%s", dst, LONG_DIR);
        assert(r<PATH_MAX);
    }
    int r = stat(dst_long_dir, &sb);

    if(source) free(source);
    if(destination) free(destination);

    free(src);
    free(dst);

    if (r) {
        fail();
    } else {
        pass();
        cleanup_dirs();
    }

    printf(": backup_sub_dirs()\n");

    return r;
}

int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    int r = backup_sub_dirs();
    return r;
}
