/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:  
/*======
This file is part of Percona TokuBackup.

Copyright (c) 2006, 2015, Percona and/or its affiliates. All rights reserved.

    Percona TokuBackup is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License, version 2,
    as published by the Free Software Foundation.

     Percona TokuBackup is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Percona TokuBackup.  If not, see <http://www.gnu.org/licenses/>.

----------------------------------------

    Percona TokuBackup is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License, version 3,
    as published by the Free Software Foundation.

    Percona TokuBackup is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with Percona TokuBackup.  If not, see <http://www.gnu.org/licenses/>.
======= */

#ident "$Id$"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "backup_test_helpers.h"
#include "backup_debug.h"

const int N_FNAMES = 2;

void* do_backups(void *v) {
    check(v==NULL);
    pthread_t thread;
    start_backup_thread(&thread);
    finish_backup_thread(thread);
    return v;
}

char *fnames[N_FNAMES];

void do_create(void) {
    printf("do_create() started\n");
    char * name0 = fnames[0];
    int fd = open(name0, O_RDWR | O_CREAT, 0777);
    if (fd >= 0) {
        int r = close(fd);
        check(r == 0);
    }

    printf("do_create() finished, created file = %s\n", name0);
    return;
}

void* do_rename(void *p) {
    printf("do_rename() started\n");
    check(p == NULL);
    char * name0 = fnames[0];
    char * name1 = fnames[1];
    int r = rename(name0, name1);
    if (r != 0) {
        perror("2nd renmae() failed.");
    }

    printf("do_rename() finished, renamed file = %s to %s\n", name0, name1);
    return p;
}

int test_two_renames(void) {
    int r = 0;
    setup_source();
    setup_destination();
    char *src = get_src();
    int len = strlen(src)+10;
    for (int i=0; i<N_FNAMES; i++) {
        fnames[i] = (char*)malloc(len);
        char letter = 'A';
        r = snprintf(fnames[i], len, "%s/%c", src, letter + (char)i);
        check(r < len);
    }

    // 1. Create the 'A' file.
    do_create();
    
    // 2. Set backup to keep capturing.
    backup_set_keep_capturing(true);

    // 3. Start backup.
    pthread_t backup_thread;
    start_backup_thread(&backup_thread);
    
    // 4. Start first rename() thread.
    pthread_t rename_thread_one;
    r = pthread_create(&rename_thread_one, NULL, do_rename, NULL);
    check(r == 0);

    // 5. Start second rename() thread. (Should block, in fixed version).
    pthread_t rename_thread_two;
    r = pthread_create(&rename_thread_two, NULL, do_rename, NULL);
    check(r == 0);

    // 7. Wait for create() then rename() threads to finish.
    r = pthread_join(rename_thread_one, NULL);
    check(r == 0);
    r = pthread_join(rename_thread_two, NULL);
    check(r == 0);
    printf("threads done...\n");

    // 8. allow backup to finish (turn keep_capturing off).
    backup_set_keep_capturing(false);

    // 9. Wait for backup to finish.
    finish_backup_thread(backup_thread);

    // 10. Verify that the source file and destination file 
    char *dst = get_dst();
    int dst_len = strlen(dst)+10;
    char * dest_name = (char*)malloc(dst_len);
    char letter = 'B';
    r = snprintf(dest_name, dst_len, "%s/%c", dst, letter);
    check(r < len);

    struct stat buf;
    r = stat(dest_name, &buf);
    if (r != 0) {
        fail();
        perror("Destination file could not be stat()'d");
        printf("deat_name = %s\n", dest_name);
    } else {
        pass(); printf("\n");
    }

    // Cleanup.
    free(dest_name);
    free(dst);

    for (int i=0; i<N_FNAMES; i++) {
        free(fnames[i]);
    }

    //    cleanup_dirs();
    free((void*)src);
    return r;
}

int test_main(int argc, const char *argv[]) {
    int ntests = 1;
    for (int i=1; i<argc; i++) {
        if (strcmp(argv[i], "--ntests") == 0 && i+1 < argc) {
            ntests = atoi(argv[++i]);
        }
    }
    int r = 0;
    for (int i=0; i<ntests; i++) {
        r = test_two_renames();
        if (r != 0)
            break;
    }
    return r;
}
