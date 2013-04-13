#include <limits.h>
#include <stdio.h>
#include <unistd.h>
#include <valgrind/helgrind.h>

#include "backup_test_helpers.h"
#include "source_file.h"

class source_file sf("hello");


volatile int stepa = 0;
volatile int stepb = 0;
volatile int stepc = 0;

static const uint64_t doit_lo = 5, doit_hi = 10;
static void* doit(void* ignore) {

    { int r = sf.lock_range(doit_lo, doit_hi);   check(r==0);  }
    stepa = 1;


    stepb = 1;
    while (!stepc);
    { int r = sf.unlock_range(doit_lo, doit_hi); check(r==0); }

    return ignore;
}

static void thread_test_block(void) {
    VALGRIND_HG_DISABLE_CHECKING(&stepa, sizeof(stepa));
    VALGRIND_HG_DISABLE_CHECKING(&stepb, sizeof(stepb));
    VALGRIND_HG_DISABLE_CHECKING(&stepc, sizeof(stepc));
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
    { int r = sf.lock_range(9,12);           check(r==0); }
    check(stepb==1); // must have gotten to stepb in the doit function.(
    
    {
        void *result;
        int r = pthread_join(th, &result);
        check(r==0);
        check(result==&the_char);
    }
    { int r = sf.unlock_range(9,12);           check(r==0); }
    VALGRIND_HG_ENABLE_CHECKING(&stepa, sizeof(stepa));
    VALGRIND_HG_ENABLE_CHECKING(&stepb, sizeof(stepb));
    VALGRIND_HG_ENABLE_CHECKING(&stepc, sizeof(stepc));
}

static void thread_test_noblock(void) {
    VALGRIND_HG_DISABLE_CHECKING(&stepa, sizeof(stepa));
    VALGRIND_HG_DISABLE_CHECKING(&stepb, sizeof(stepb));
    VALGRIND_HG_DISABLE_CHECKING(&stepc, sizeof(stepc));
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
    { int r = sf.lock_range(12,15);             check(r==0); }
    
    stepc = 1;
    
    {
        void *result;
        int r = pthread_join(th, &result);
        check(r==0);
        check(result==&the_char);
    }

    { int r = sf.unlock_range(12,15);           check(r==0); }
    VALGRIND_HG_ENABLE_CHECKING(&stepa, sizeof(stepa));
    VALGRIND_HG_ENABLE_CHECKING(&stepb, sizeof(stepb));
    VALGRIND_HG_ENABLE_CHECKING(&stepc, sizeof(stepc));
}

int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {

    // test a single range that covers everything
    {   int r = sf.init();                          check(r==0); }
    {   int r = sf.lock_range(0, LLONG_MAX);        check(r==0); }
    check( sf.lock_range_would_block_unlocked(0, 1));
    check(!sf.lock_range_would_block_unlocked(0, 0));
    check( sf.lock_range_would_block_unlocked(10, 100));
    check(!sf.lock_range_would_block_unlocked(10, 10));
    check( sf.lock_range_would_block_unlocked(LLONG_MAX-1, LLONG_MAX));
    check(!sf.lock_range_would_block_unlocked(LLONG_MAX, LLONG_MAX));
    {   int r = sf.unlock_range(0, LLONG_MAX);      check(r==0); }


    // Test two ranges that are adjacent.
    {   int r = sf.lock_range(10, 20);              check(r==0); }
    {   int r = sf.lock_range(20, 30);              check(r==0); }
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
    {   int r = sf.lock_range(10, 20);              check(r==0); }
    {   int r = sf.lock_range(30, 40);              check(r==0); }
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
