/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "backup.h"
#include "backup_test_helpers.h"

const char * const WRITTEN_STR = "goodbye\n";
const int WRITTEN_STR_LEN = 8;

//
static int verify(void)
{
    int r = 0;
    int backup_fd = open(DST "/bar.data", O_RDONLY);
    assert(backup_fd >= 0);
    char backup_string[30] = {0};
    r = read(backup_fd, backup_string, WRITTEN_STR_LEN);
    assert(r == WRITTEN_STR_LEN);
    r = strcmp(WRITTEN_STR, backup_string);
    return r;
}

//
void open_write_close()
{
    int result = 0;
    setup_source();
    setup_dirs();
    setup_destination();
    add_directory(SRC, DST);
    start_backup();
    
    int fd = 0;
    fd = open(SRC "/bar.data", O_WRONLY);
    assert(fd >= 0);
    result = write(fd, WRITTEN_STR, WRITTEN_STR_LEN);
    assert(result == 8);
    result = close(fd);
    assert(result == 0);

    if(verify()) {
        fail();
    } else {
        pass();
    }
    printf(": open_write_close()\n");

    stop_backup();
}
