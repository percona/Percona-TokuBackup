/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "backup.h"
#include "backup_test_helpers.h"

//
void test_truncate()
{
    int result = 0;
    int fd = 0;
    setup_source();
    setup_dirs();
    setup_destination();

    add_directory(SRC, DST);
    start_backup();

    // Create a new file.
    fd = open(SRC "/my.data", O_CREAT | O_RDWR, 0777);
    assert(fd >= 0);

    // Write a large chunk of data to it.
    result = write(fd, "Hello World", 12);

    // Truncate the end off.
    int r = ftruncate(fd, 6);
    const int SIZE = 20;
    char buf_source[SIZE];
    result = pread(fd, buf_source, 20, 0);

    // Confirm that the both files are the same size.
    int backup_fd = open(DST "/my.data", O_RDWR);
    char buf_copy[SIZE];
    result = read(backup_fd, buf_copy, SIZE);
    result = strcmp(buf_source, buf_copy);

    if (result != 0) { 
        fail();
        printf(": BACKUP: <TEST> Files don't match! %s != %s\n", buf_source, buf_copy);
        fail();
    } else {
        pass();
    }

    printf(": test_truncate()\n");
    
    stop_backup();
}
