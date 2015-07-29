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

volatile long counter = 2; // so we know that all the work is done.

char *src; 

void* do_backup(void *v) {
    check(v==NULL);
    tokubackup_throttle_backup(1); // very slow
    pthread_t thread;
    start_backup_thread(&thread);
    while (counter>0) sched_yield();
    tokubackup_throttle_backup(0xFFFFFFFFF); // reasonably fast
    finish_backup_thread(thread);
    return v;
}

void* do_writes(void *v) {
    check(v==NULL);
    int fd = openf(O_RDWR | O_CREAT, 0777, "%s/wfile", src);
    check(fd>=0);
    int wcount=0;
    while (counter>1) { // do until all the opens are done
        char buf[100];
        int l = snprintf(buf, sizeof(buf), "%d\n", wcount);
        check((size_t)l<sizeof(buf));
        size_t r = write(fd, buf, l);
        check(r==(size_t)l);
        wcount++;
        printf(".");
    }
    __sync_fetch_and_add(&counter, -1); // let do_backups know we finished.
    return v;
}    


void* do_opens(void *v) {
    check(v==NULL);
    const int n_opens = 129;
    int fds[n_opens];
    for (int i=0; i<n_opens; i++) {
        fds[i] = openf(O_RDWR | O_CREAT, 0777, "%s/%d", src, i);
        check(fds[i]>=0);
        usleep(1000);
    }
    for (int i=0; i<n_opens; i++) {
        int r = close(fds[i]);
        check(r==0);
    }
    __sync_fetch_and_add(&counter, -1); // done doing the opens
    return v;
}


int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    setup_source();
    setup_destination();
    src = get_src();
    {
        int fd = openf(O_WRONLY | O_CREAT, 0777, "%s/somedata", src);
        check(fd>=0);
        unsigned char buf[1024];
        for (size_t i=0; i<sizeof(buf); i++) buf[i]=i%256;
        for (int j=0; j<1024; j++) {
            ssize_t r = write(fd, buf, sizeof(buf));
            check(r==sizeof(buf));
        }
        int r = close(fd);
        check(r==0);
    }
        
    // Run the backups, and they throttle to very slow until the writes are done.
    pthread_t backups;
    {
        int r = pthread_create(&backups, NULL, do_backup, NULL);
        check(r==0);
    }
    usleep(1000);

    // The writes run until the opens are done.  But we give them a little bit of a head start with the usleep.
    pthread_t writes;
    {
        int r = pthread_create(&writes, NULL, do_writes, NULL);
        check(r==0);
    }
    usleep(1000);

    // opens do a bunch of opens, and they do usleeps between so that the writes get a chance.
    pthread_t opens;
    {
        int r = pthread_create(&opens, NULL, do_opens, NULL);
        check(r==0);
    }
    
    {
        void *v;
        int r = pthread_join(backups, &v);
        check(r==0 && v==NULL);
    }
    {
        void *v;
        int r = pthread_join(writes, &v);
        check(r==0 && v==NULL);
    }
    {
        void *v;
        int r = pthread_join(opens, &v);
        check(r==0 && v==NULL);
    }
    cleanup_dirs();
    free(src);
    return 0;
}
