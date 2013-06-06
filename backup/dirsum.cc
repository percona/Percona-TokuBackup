/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

// Walk a directory and return the total size.

#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>

#include "raii-malloc.h"
#include "backup_internal.h"
#include "check.h"

long long dirsum(const char *dname) throw() {
    DIR *dir = opendir(dname);
    if (dir==0) return 0;
    long long sum = 0;
    while (1) {
        struct dirent *dent = readdir(dir);
        if (dent==0) break;
        if (!strcmp(dent->d_name, "." )) continue;
        if (!strcmp(dent->d_name, "..")) continue;
        int len = strlen(dname) + 1 + strlen(dent->d_name) + 1; // one for the slash, one for the NUL.
        with_object_to_free<char *> str((char*)malloc(len));
        {
            int r = snprintf(str.value, len, "%s/%s", dname, dent->d_name);
            check(r==len-1);
        }
	struct stat st;
        int r = lstat(str.value, &st);
        if (r == -1) continue;
        if (S_ISLNK(st.st_mode)) continue;
        if (S_ISDIR(st.st_mode)) {
            long long sub_dirsum = dirsum(str.value);
            sum += sub_dirsum;
            continue;
        }
        sum += st.st_size;
    }
    closedir(dir);
    return sum;
}

