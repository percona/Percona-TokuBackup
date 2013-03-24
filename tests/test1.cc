/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>

#include "backup.h"

#define SRC "b2_src"
#define DST "b2_dst"

//
static void setup_destination()
{
    system("rm -rf " DST);
}

//
static void setup_source()
{
    system("rm -rf " SRC);
    system("mkdir " SRC);
    system("touch " SRC "/foo");
    system("echo hello > " SRC "/bar.data");
    system("mkdir " SRC "/subdir");
    system("echo there > " SRC "/subdir/sub.data");
}

//
int main(int argc, char *argv[])
{
    argc = argc;
    argv = argv;
    int result = 0;
    setup_source();
    setup_destination();
    add_directory(SRC, DST);
    start_backup();
    
    int fd = 0;
    fd = open(SRC "/bar.data", O_WRONLY);
    assert(fd >= 0);
    result = write(fd, "goodbye\n", 8);
    assert(result == 8);
    result = close(fd);
    assert(result == 0);

    stop_backup();
    return result;
}
