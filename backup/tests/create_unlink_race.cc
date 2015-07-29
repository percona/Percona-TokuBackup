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

#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "backup_test_helpers.h"
#include "backup_debug.h"

const int N=10;
const int N_FNAMES = 1;

void* do_backups(void *v) {
    check(v==NULL);
    pthread_t thread;
    start_backup_thread(&thread);
    finish_backup_thread(thread);
    return v;
}

char *fnames[N_FNAMES];

void* do_create(void *p) {
    printf("do_create() started\n");
    check(p == NULL);
    char * name0 = fnames[0];
    int fd = open(name0, O_RDWR | O_CREAT, 0777);
    if (fd >= 0) {
        printf("do_create() finished, created file = %s\n", name0);
        int r = close(fd);
        check(r == 0);
    }

    return p;
}

void* do_unlink(void *p) {
    printf("do_unlink() started\n");
    check(p == NULL);
    char * name0 = fnames[0];
    int r = unlink(name0);
    check(r == 0);
    printf("do_unlink() finished, file = %s\n", name0);
    return p;
}

int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
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

    // 1. Set pause points
    HotBackup::toggle_pause_point(HotBackup::OPEN_DESTINATION_FILE);
    
    // 2. Set backup to keep capturing.
    backup_set_keep_capturing(true);

    // 3. Start backup.
    pthread_t backup_thread;
    start_backup_thread(&backup_thread);
    sleep(1);

    // 4. Start create() thread.
    pthread_t create_thread;
    r = pthread_create(&create_thread, NULL, do_create, NULL);
    check(r == 0);
    sleep(1);
    
    // 5. Start rename() thread. (Should block, in fixed version).
    pthread_t unlink_thread;
    r = pthread_create(&unlink_thread, NULL, do_unlink, NULL);
    check(r == 0);

    // 6. Sleep, then turn off open() pause point.
    HotBackup::toggle_pause_point(HotBackup::OPEN_DESTINATION_FILE);

    // 7. Wait for create() thread to finish.
    r = pthread_join(create_thread, NULL);
    check(r == 0);
    r = pthread_join(unlink_thread, NULL);
    check(r == 0);
    printf("threads done...\n");

    // 9. allow backup to finish (turn keep_capturing off).
    backup_set_keep_capturing(false);

    // 10. Wait for backup to finish.
    finish_backup_thread(backup_thread);

    // 11. Verify that the source file and destination file 
    char *dst = get_dst();
    int dst_len = strlen(dst)+10;
    char * dest_name = (char*)malloc(dst_len);
    char letter = 'A';
    r = snprintf(dest_name, dst_len, "%s/%c", dst, letter);
    check(r < len);

    struct stat buf;
    r = stat(dest_name, &buf);
    if (r != 0) {
        pass(); printf("\n");
        r = 0;
    } else {
        fail();
        printf("destination file should NOT exist:  %s\n", dest_name);
        r = -1;
    }

    // Cleanup.
    free(dest_name);
    free(dst);

    for (int i=0; i<N_FNAMES; i++) {
        free(fnames[i]);
    }

    cleanup_dirs();
    free((void*)src);
    return r;
}
