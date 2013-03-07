/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:   

#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>

#include "backup_integration_tests.h"
#include "backup_test_helpers.h"

int main(int argc, char *argv[])
{
    int result = 0;
    cleanup_dirs();

    // Components:
    backup_sub_dirs();
    copy_files();                     

    // With Active Copy:
    open_write_close();
    //    read_and_seek();
    test_truncate();

    cleanup_dirs();
    return result;
}
