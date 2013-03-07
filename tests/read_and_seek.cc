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

void read_and_seek()
{
    int result = 0;
    int fd = 0;
    setup_source();
    setup_dirs();
    setup_destination();
    add_directory(SRC, DST);
    start_backup();

    fd = open(SRC "/my.data", O_CREAT | O_RDWR);
    assert(fd >= 0);
    result = write(fd, "Hello World\n", 12);
    char buf[10];
    result = read(fd, buf, 5);
    if (result < 0) {
        perror("read failed.");
        abort();
    }

    result = lseek(fd, 1, SEEK_CUR);
    assert(result > 0);
    result = write(fd, "Cruel World\n", 12);
    assert(result > 0);
    result = close(fd);
    assert(result == 0);

    stop_backup();
}
