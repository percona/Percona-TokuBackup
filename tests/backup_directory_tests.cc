/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

#include "backup_test_helpers.h"
#include "backup.h"
#include "backup_directory.h"


#define LONG_DIR "/ThisIsALongDirectory/WithNothing/InIt/"

int backup_sub_dirs(void)
{
    int result = 0;
    setup_destination();
    setup_source();

    char *source = realpath("./" SRC, NULL);
    char *destination = realpath("./" DST, NULL);

    // We need to pretend that there is a source directory with this
    // long path.  So, let's first create enough space for it.
    char *newpath = (char*)malloc(PATH_MAX);
    strcpy(newpath, destination);
    char *temp = newpath;

    // Move to the end of the absolute destination path we just
    // copied.
    while(*temp++) {;}
    --temp;

    // Append the long directory path to the destination path we just
    // copied.
    const char *longname = LONG_DIR;
    int i = 0;
    while(*longname) {
        *temp = *(longname)++;
        temp++;
    }

    *temp = 0;
    backup_directory dir;
    dir.set_directories(source, destination);
    dir.create_subdirectories(newpath);

    // Verify:
    struct stat sb;
    int r = stat("./" DST LONG_DIR, &sb);
    if (r) {
        fail();
    } else {
        pass();
    }

    printf(": backup_sub_dirs()\n");

    if(source) free(source);
    if(destination) free(destination);

    return result;
}
