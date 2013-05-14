/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

/* A race between open() and prepare(). */

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include "backup_test_helpers.h"

static char *src;

static volatile int n_backups_done = 0;
static const int n_backups_to_do = 4;

static void* open_close_loop(void * ignore) {
    while (n_backups_done < n_backups_to_do) {
        int fd = openf(O_RDONLY|O_CREAT, 0777, "%s/file", src);
        check(fd>=0);
        int r = close(fd);
        check(r==0);
    }
    return ignore;
}

int test_main(int argc, const char *argv[] __attribute__((__unused__))) {
    check(argc==1);
    setup_source();
    src = get_src();
    char *dst = get_dst();
    pthread_t th;
    char ignore[1];
    {
        int r = pthread_create(&th, NULL, open_close_loop, ignore);
        check(r==0);
    }
    for (int i=0; i<n_backups_to_do; i++) {
        pthread_t bth;
        setup_destination();
        start_backup_thread(&bth);
        finish_backup_thread(bth);
        __sync_fetch_and_add(&n_backups_done, 1);
    }
    {
        void *result;
        int r = pthread_join(th, &result);
        check(r==0);
        check(result==ignore);
    }
    free(src);
    free(dst);
    return 0;
}
    
