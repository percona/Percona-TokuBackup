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

#include <limits.h>
#include <stdio.h>
#include <unistd.h>
#include "backup_helgrind.h"

#include "backup_test_helpers.h"
#include "source_file.h"

class source_file sf("hello");

volatile int stepa = 0;
volatile int stepb = 0;
volatile int stepc = 0;

static const uint64_t doit_lo = 5, doit_hi = 10;
static void* doit(void* ignore) {

    sf.lock_range(doit_lo, doit_hi);
    stepa = 1;


    stepb = 1;
    while (!stepc);
    { int r = sf.unlock_range(doit_lo, doit_hi); check(r==0); }

    return ignore;
}

static void thread_test_block(void) {
    TOKUBACKUP_VALGRIND_HG_DISABLE_CHECKING(&stepa, sizeof(stepa));
    TOKUBACKUP_VALGRIND_HG_DISABLE_CHECKING(&stepb, sizeof(stepb));
    TOKUBACKUP_VALGRIND_HG_DISABLE_CHECKING(&stepc, sizeof(stepc));
    stepa = stepb = 0;
    pthread_t th;
    char the_char;
    {
        int r = pthread_create(&th, NULL, doit, &the_char);
        check(r==0);
    }

    while (!stepa); // wait for stepa to finish.
    // Now 5,10 is blocked
    stepc = 1; // let him go ahead and run
    sf.lock_range(9,12);
    check(stepb==1); // must have gotten to stepb in the doit function.(
    
    {
        void *result;
        int r = pthread_join(th, &result);
        check(r==0);
        check(result==&the_char);
    }
    { int r = sf.unlock_range(9,12);           check(r==0); }
    TOKUBACKUP_VALGRIND_HG_ENABLE_CHECKING(&stepa, sizeof(stepa));
    TOKUBACKUP_VALGRIND_HG_ENABLE_CHECKING(&stepb, sizeof(stepb));
    TOKUBACKUP_VALGRIND_HG_ENABLE_CHECKING(&stepc, sizeof(stepc));
}

static void thread_test_noblock(void) {
    TOKUBACKUP_VALGRIND_HG_DISABLE_CHECKING(&stepa, sizeof(stepa));
    TOKUBACKUP_VALGRIND_HG_DISABLE_CHECKING(&stepb, sizeof(stepb));
    TOKUBACKUP_VALGRIND_HG_DISABLE_CHECKING(&stepc, sizeof(stepc));
    stepa = stepb = stepc = 0;
    pthread_t th;
    char the_char;
    {
        int r = pthread_create(&th, NULL, doit, &the_char);
        check(r==0);
    }
    while (!stepa); // wait for stepa to finish
    // Now 5,10 is blocked.
    // this will deadlock of the lock blocks.
    sf.lock_range(12,15);
    
    stepc = 1;
    
    {
        void *result;
        int r = pthread_join(th, &result);
        check(r==0);
        check(result==&the_char);
    }

    { int r = sf.unlock_range(12,15);           check(r==0); }
    TOKUBACKUP_VALGRIND_HG_ENABLE_CHECKING(&stepa, sizeof(stepa));
    TOKUBACKUP_VALGRIND_HG_ENABLE_CHECKING(&stepb, sizeof(stepb));
    TOKUBACKUP_VALGRIND_HG_ENABLE_CHECKING(&stepc, sizeof(stepc));
}

int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {

    // test a single range that covers everything
    sf.lock_range(0, LLONG_MAX);
    check( sf.lock_range_would_block_unlocked(0, 1));
    check(!sf.lock_range_would_block_unlocked(0, 0));
    check( sf.lock_range_would_block_unlocked(10, 100));
    check(!sf.lock_range_would_block_unlocked(10, 10));
    check( sf.lock_range_would_block_unlocked(LLONG_MAX-1, LLONG_MAX));
    check(!sf.lock_range_would_block_unlocked(LLONG_MAX, LLONG_MAX));
    {   int r = sf.unlock_range(0, LLONG_MAX);      check(r==0); }


    // Test two ranges that are adjacent.
    sf.lock_range(10, 20);
    sf.lock_range(20, 30);
    check(!sf.lock_range_would_block_unlocked(0, 10));
    for (int i=10; i<30; i++) {
        for (int j=i+1; j<=30; j++) {
            check(sf.lock_range_would_block_unlocked(i, j));
        }
    }
    check(!sf.lock_range_would_block_unlocked(30, LLONG_MAX));
    {   int r = sf.unlock_range(10, 20);            check(r==0); }
    {   int r = sf.unlock_range(20, 30);            check(r==0); }

    // test two ranges with a gap in between.
    sf.lock_range(10, 20);
    sf.lock_range(30, 40);
    for (int i=0; i<50; i++) {
        for (int j=i; j<50; j++) {
            bool expect_block =
                    (i<j) && // i,j is not empty  and
                    (((i<20) && (j>10)) || // i,j intersects the first range or
                     ((i<40) && (j>30)));  // i,j intersects the second range
            check(expect_block == sf.lock_range_would_block_unlocked(i,j));
        }
    }
    {   int r = sf.unlock_range(10, 20);            check(r==0); }
    {   int r = sf.unlock_range(30, 40);            check(r==0); }

    
    // Now test to verify that if there is no lock conflict then the blocking actually happens properly.
    thread_test_block();

    // And if there is a lock conflict, then blocking happens properly.
    thread_test_noblock();


    return 0;
}
