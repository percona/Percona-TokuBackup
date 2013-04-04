/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id: open_write_close.cc 54729 2013-03-26 16:24:56Z bkuszmaul $"

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
#include "backup_debug.h"

const int SIZE = 100;
static int fd;
const char ONE = 'a';
const char ZERO = 'b';

static void* write_ones(void *p __attribute__((__unused__))) {
    void *ret = NULL;
    char data[SIZE] = {ONE};    
    int r = write(fd, data, SIZE);
    assert(r == SIZE);
    return ret;
}

static int write_race(void) {
    int result = 0;

    // 1. Create one file in source with a few bytes.
    char file[100];
    char *free_me = get_src();
    snprintf(file, 100, "%s/%s", free_me, "MyFile.txt");
    fd = open(file, O_CREAT | O_RDWR, 0777);
    assert(fd >= 0);
    char data[SIZE] = {ZERO};
    int r = write(fd, data, SIZE);
    assert(r == SIZE);

    // 2. Set Pause point to after read but before write
    HotBackup::set_pause(HotBackup::COPIER_AFTER_READ_BEFORE_WRITE);

    // 3. Start backup.
    pthread_t backup_thread;
    start_backup_thread(&backup_thread);

    // 6. start new thread that writes to the first few bytes.
    pthread_t write_thread;
    r = pthread_create(&write_thread, NULL, write_ones, NULL);
    assert(r == 0);

    // 7. Sleep for a little while to give the thread a chance to write and/or get blocked.
    sleep(1);

    // 8. Turn off pause point.
    HotBackup::set_pause(~HotBackup::COPIER_AFTER_READ_BEFORE_WRITE);
    r = pthread_join(write_thread, NULL);
    assert(r == 0);
    finish_backup_thread(backup_thread);

    // 9. Check that backup copy and source copy match.
    char buf[SIZE];
    r = pread(fd, buf, SIZE, 0);
    assert(r == SIZE);
    if (buf[0] == ZERO) {
        fail();
        printf("\nCharacters in Buffers don't match: %c vs %c\n", buf[0], ONE);
        result = -1;
    } else {
        pass();
        result = 0;
    }

    // 10. cleanup:
    close(fd);
    free((void*)free_me);
    return result;
}

int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    int result = 0;
    setup_source();
    setup_destination();
    result = write_race();
    cleanup_dirs();
    return result;
}
