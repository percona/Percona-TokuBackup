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

const char *BACKUP_NAME = __FILE__;

//
static int verify(void)
{
    char *dst = get_dst();
    int backup_fd = openf(O_RDONLY, 0, "%s/bar.data", dst);
    assert(backup_fd >= 0);    
    free(dst);
    char backup_string[30] = {0};
    int r = read(backup_fd, backup_string, WRITTEN_STR_LEN);
    assert(r == WRITTEN_STR_LEN);
    r = strcmp(WRITTEN_STR, backup_string);
    return r;
}

//
static void open_write_close(void) {
    setup_source();
    setup_dirs();
    setup_destination();
    pthread_t thread;
    start_backup_thread(&thread);

    char *src = get_src();
    int fd = openf(O_WRONLY, 0, "%s/bar.data", src);
    assert(fd >= 0);
    free(src);
    int result = write(fd, WRITTEN_STR, WRITTEN_STR_LEN);
    assert(result == 8);
    result = close(fd);
    assert(result == 0);

    finish_backup_thread(thread);

    if(verify()) {
        fail();
    } else {
        pass();
    }
    printf(": open_write_close()\n");
}

int main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    open_write_close();
    return 0;
}
