/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#include <stdio.h>
#include <stdlib.h>
#include "backup_test_helpers.h"

const char * const DEFAULT_TERM = "\033[0m";
const char * const RED_TERM = "\033[31m";
const char * const GREEN_TERM = "\033[32m";

void pass()
{
    printf(GREEN_TERM);
    printf("[PASS]");
    printf(DEFAULT_TERM);}

void fail()
{
    printf(RED_TERM);
    printf("[FAIL]");
    printf(DEFAULT_TERM);
}

void setup_destination()
{
    system("rm -rf " DST);
    system("mkdir " DST);
}

void setup_source()
{
    system("rm -rf " SRC);
    system("mkdir " SRC);
}

void setup_dirs()
{
    system("touch " SRC "/foo");
    system("echo hello > " SRC "/bar.data");
    system("mkdir " SRC "/subdir");
    system("echo there > " SRC "/subdir/sub.data");
}

void cleanup_dirs()
{
    system("rm -rf " DST);
    system("rm -rf " SRC);
}
