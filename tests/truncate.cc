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
void test_truncate(void) {
    int result = 0;
    int fd = 0;
    setup_source();
    setup_dirs();
    setup_destination();

    pthread_t thread;
    start_backup_thread(&thread);

    // Create a new file.
    char *src = get_src();
    fd = openf(O_CREAT | O_RDWR, 0777, "%s/my.data", src);
    assert(fd >= 0);
    free(src);

    // Write a large chunk of data to it.
    result = write(fd, "Hello World", 12);

    // Truncate the end off.
    {
        int r = ftruncate(fd, 6);
        assert(r==0);
    }
    const int SIZE = 20;
    char buf_source[SIZE];
    result = pread(fd, buf_source, 20, 0);

    finish_backup_thread(thread);

    // Confirm that the both files are the same size.
    char *dst = get_dst();
    int backup_fd = openf(O_RDWR, 0, "%s/my.data", dst);
    assert(backup_fd>=0);
    free(dst);
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
}

const char *BACKUP_NAME = __FILE__;

int main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    test_truncate();
    return 0;
}
