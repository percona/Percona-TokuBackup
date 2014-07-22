/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "backup.h"
#include "backup_test_helpers.h"

const int TRIES = 4;
const int LEN = 26;
char *dirs[TRIES];

/**** 
static void setup(void)
{
    for (int i = 0; i < TRIES; ++i) {
        char s[LEN];
        int r = snprintf(s, sizeof(s), "multiple_backups.backup_%d", i);
        check(r<(int)sizeof(s));
        systemf("rm -rf %s", s);
        dirs[i] = strdup(s);
        check(dirs[i]);
    }
}
****/
/**** 

static void breakdown(void)
{
    // The backup thread helpers free dirs[i].
}
****/

static void write_lots_of_dummy_data(char *src)
{
    const int SIZE = 100;
    const int MB = 1024 * 1024;
    char dummy[MB] = {0};
    char file[SIZE];
    snprintf(file, SIZE, "%s/%s", src, "dummy");
    int fd = open(file, O_CREAT | O_RDWR, 0777);
    check(fd >= 0);
    const int MEGS = 10;
    for (int i = 0; i < MEGS; ++i) {
        int r = write(fd, dummy, MB);
        check(r == MB);
    }
}

static void work_it(char *src, char *magic, int size)
{
    char file[100];
    snprintf(file, 100, "%s/%s", src, magic);

    // 1.  Create a file.
    int fd = open(file, O_CREAT | O_RDWR, 0777);
    check(fd >= 0);
    // 2.  Write magic to the file.
    int r = write(fd, magic, size);
    check(r == size);

    // 3.  Close the file.
    close(fd);

    // 4.  Open the file.
    fd = open(file, O_RDWR);

    // 5.  Read from the file.
    char *buf = (char*)malloc(sizeof(char) * size);
    r = read(fd, buf, size);

    // 6.  Write magic to the file.
    lseek(fd, size, SEEK_SET);
    write(fd, magic, size);

    // 7.  pwrite magic to the file.
    pwrite(fd, magic, size, size * 3);

    // 8.  ftruncate magic from the file.
    ftruncate(fd, size * 3);

    // close the file.
    close(fd);
    free((void*)buf);
}

static int check_it(char *dst, char *magic, int size, int count)
{
    int r = 0;
    int backup_fd = openf(O_RDONLY, 0, "%s/magic%d", dst, count);
    char backup_buf[20] = {0};
    pread(backup_fd, backup_buf, size, 0);
    char magic_buf[20] = {0};
    snprintf(magic_buf, size, "%s%d", magic, count);
    int result = strcmp(backup_buf, magic_buf);
    if (result != 0) {
        printf("Couldn't match: source:%s vs backup:%s\n", magic_buf, backup_buf);
        r = -1;
    }

    return r;
}

/**** 
//
static int many_directories(void) {
    setup_source();
    setup();
    char * src = get_src();
    write_lots_of_dummy_data(src);

    int result = 0;

    pthread_t thread;
    tokubackup_throttle_backup(1L << 22);
    for (int i = 0; i < TRIES; ++i) {
        setup_directory(dirs[i]);
        start_backup_thread(&thread, dirs[i]);
        const int SIZE = 7;
        char buf[SIZE] = {0};
        snprintf(buf, SIZE, "magic%i", i);
        work_it(src, buf, SIZE);
        finish_backup_thread(thread);
        snprintf(buf, SIZE, "magic");
        int r = check_it(buf, SIZE, i);
        if (r != 0) {
            result = -1 * i;
        }
    }

    free(src);
    if(result < 0) {
        printf("Backup # %d failed.\n", result * -1);
        fail();
        result = -1;
    } else {
        pass();
    }

    breakdown();

    return result;
}
****/

int many_directories(const int directory_count) {
    int result = 0;

    // Set up all the directories.
    set_dir_count(directory_count);
    char **sources = (char**)malloc(sizeof(char**) * directory_count);
    char **destinations = (char**)malloc(sizeof(char**) * directory_count);
    for (int i = 0; i < directory_count; ++i)
    {
        sources[i] = get_src(i);
        destinations[i] = get_dst(i);
        setup_directory(sources[i]);
        setup_directory(destinations[i]);
        write_lots_of_dummy_data(sources[i]);        
    }

    pthread_t thread; 
    start_backup_thread(&thread);
    // Do work in each directory.
    const int SIZE = 7;
    char buf[SIZE] = {0};
    for (int i = 0; i < directory_count; ++i)
    {
        snprintf(buf, SIZE, "magic%i", i);
        work_it(sources[i], buf, SIZE);
    }

    finish_backup_thread(thread);

    // Check the work done in each directory.
    for (int i = 0; i < directory_count; ++i)
    {
        snprintf(buf, SIZE, "magic");
        int r = check_it(destinations[i], buf, SIZE, i);
        if (r != 0) {
            result = -1 * i;
        }
    }

    // Don't forget to deallocate the strings!
    for (int i = 0; i < directory_count; ++i)
    {
        free((void*)sources[i]);
        free((void*)destinations[i]);
    }

    return result;
}

int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    int r = 0;
    const int MAX_DIRS = 8;
    for (int i = 1; i <= MAX_DIRS; i = i << 1)
    {
        r = many_directories(i);
        if (r != 0)
        {
            break;
        }
    }

    return r; 
}
