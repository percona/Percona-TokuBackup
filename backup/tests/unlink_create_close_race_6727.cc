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
#include <unistd.h>

#include "backup_test_helpers.h"

const int N=10;
const int N_BACKUP_ITERATIONS = 100;
const int N_OP_ITERATIONS = 10000;
const int N_FNAMES = 2;

volatile long counter = N+1;

void* do_backups(void *v) {
    check(v==NULL);
    for (int i=0; i<N_BACKUP_ITERATIONS; i++) {
        pthread_t thread;
        start_backup_thread(&thread);
        finish_backup_thread(thread);
    }
    __sync_fetch_and_add(&counter, -1);
    while (counter>0) {
        pthread_t thread;
        start_backup_thread(&thread);
        finish_backup_thread(thread);
        sched_yield();
    }
    return v;
}

char *fnames[N_FNAMES];

volatile int rename_results[2], open_results[2], unlink_results[2];

void do_client_once(void) {
    int ra = random();
    const int casesize = 3;
    char *name0 = fnames[(ra/casesize)%N_FNAMES];
    char *name1 = fnames[(ra/(casesize*N_FNAMES))%N_FNAMES];
    switch(ra%casesize) {
        case 0: {
            int r = rename(fnames[(ra/4)%N_FNAMES], name1);  // possibly rename the same file to itself
            __sync_fetch_and_add(&rename_results[r==0 ? 0 : 1], 1);
            break;
        }
        case 1: {
            int excl_flags = (ra/8)%2 ? O_EXCL : 0;
            int fd = open(name0, excl_flags | O_RDWR | O_CREAT, 0777);
            if (fd>=0) {
                int r = close(fd);
                check(r==0);
            }
            __sync_fetch_and_add(&open_results[fd>=0 ? 0 : 1], 1);
            break;
        }
        case 2: {
            int r = unlink(name0);
            __sync_fetch_and_add(&unlink_results[r==0 ? 0 : 1], 1);
            break;
        }
    }
}

void* do_client(void *v) {
    int me = *(int*)v;
    check(0<=me && me<N);
    for (int i=0; i<N_OP_ITERATIONS; i++) {
        do_client_once();
    }
    __sync_fetch_and_add(&counter, -1);
    while (counter>0) {
        do_client_once();
    }
    return v;
}

int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    setup_source();
    setup_destination();
    char *src = get_src();
    int len = strlen(src)+10;
    for (int i=0; i<N_FNAMES; i++) {
        fnames[i] = (char*)malloc(len);
        int r = snprintf(fnames[i], len, "%s/A", src);
        check(r<len);
    }
    pthread_t backups;
    {
        int r = pthread_create(&backups, NULL, do_backups, NULL);
        check(r==0);
    }
    pthread_t clients[N];
    int       id[N];
    for (int i=0; i<N; i++) {
        id[i] = i;
        int r = pthread_create(&clients[i], NULL, do_client, &id[i]);
        check(r==0);
    }
    {
        void *v;
        int r = pthread_join(backups, &v);
        check(r==0 && v==NULL);
    }
    for (int i=0; i<N; i++) {
        void *v;
        int r = pthread_join(clients[i], &v);
        check(r==0 && v==(void*)&id[i]);
    }
    cleanup_dirs();
    printf("unlink  %d ok, %d failed\n", unlink_results[0], unlink_results[1]);
    printf("open    %d ok, %d failed\n", open_results[0],   open_results[1]);
    printf("rename  %d ok, %d failed\n", rename_results[0], rename_results[1]);
    free(src);
    for (int i=0; i<N_FNAMES; i++) {
        free(fnames[i]);
    }
    return 0;
}
