/* Inject enospc. */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include "backup_test_helpers.h"
#include "backup_internal.h"
#include "real_syscalls.h"

static bool disable_injections = true;
static std::vector<long> injection_pattern; // On the Kth pwrite or write, return ENOSPC, if K is in this vector.
static long injection_write_count = 0;

static char *src;
static char *dst;

static bool inject_this_time(const char *path) {
    printf("src=%s path=%s\n", dst, path);
    if (strncmp(dst, path, strlen(dst))!=0) return false;
    long old_count = __sync_fetch_and_add(&injection_write_count,1);
    for (size_t i=0; i<injection_pattern.size(); i++) {
        if (injection_pattern[i]==old_count) {
            return true;
        }
    }
    return false;
}

static mkdir_fun_t original_mkdir;

static int my_mkdir(const char *path, mode_t mode) {
    fprintf(stderr, "Doing mkdir(%s, %d)\n", path, mode); // ok to do a write, since we aren't further interposing writes in this test.
    if (inject_this_time(path)) {
        fprintf(stderr, "Injecting error\n");
        errno = ENOSPC;
        return -1;
    } else {
        return original_mkdir(path, mode);
    }
}

static int ercount=0;
static void my_error_fun(int e, const char *s, void *ignore) {
    assert(ignore==NULL);
    ercount++;
    fprintf(stderr, "Got error %d (I expected errno=%d) (%s)\n", e, ENOSPC, s);
}
    
static void testit(void) {
    disable_injections = true;
    injection_write_count = 0;

    setup_source();
    setup_dirs();
    setup_destination();

    disable_injections = false;

    backup_set_start_copying(false);
    backup_set_keep_capturing(true);

    pthread_t thread;

    start_backup_thread_with_funs(&thread,
                                  get_src(), get_dst(),
                                  simple_poll_fun, NULL,
                                  my_error_fun, NULL,
                                  ENOSPC);
    while(!backup_is_capturing()) sched_yield(); // wait for the backup to be capturing.
    fprintf(stderr, "The backup is supposedly capturing\n");
    {
        char s[1000];
        snprintf(s, sizeof(s), "%s/dir0", src);
        int r = mkdir(s, 0777);
        assert(r==0);
    }

    {
        char s[1000];
        snprintf(s, sizeof(s), "%s/dir0/dir1", src);
        int r = mkdir(s, 0777);
        assert(r==0);
    }
    fprintf(stderr,"About to start copying\n");
    backup_set_start_copying(true);

    backup_set_keep_capturing(false);
    finish_backup_thread(thread);
}


int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    src = get_src();
    dst = get_dst();
    original_mkdir  = register_mkdir(my_mkdir);

    injection_pattern.push_back(0);
    testit();
    
    printf("2nd test\n");
    injection_pattern.resize(0);
    injection_pattern.push_back(1);
    testit();

    printf("3rd test\n");
    injection_pattern.resize(0);
    injection_pattern.push_back(2);
    testit();

    free(src);
    free(dst);
    return 0;
}
