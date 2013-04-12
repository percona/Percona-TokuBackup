// Test to make sure that if the destination isn't empty, then the backup fails.  For

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "backup_test_helpers.h"

static char *src;
static char *dst;

int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    src = get_src();
    dst = get_dst();
    setup_source();
    setup_destination();
    setup_dirs();

    {
        int r = systemf("echo hello > %s/a_file_is_there_but_shouldnt_be", src);
        assert(r!=-1);
        assert(WIFEXITED(r));
        assert(WEXITSTATUS(r)==0);
    }

    pthread_t thread;
    start_backup_thread_with_funs(&thread,
                                  strdup(src), strdup(dst), // free's src and dst
                                  dummy_poll, NULL,
                                  dummy_error, NULL,
                                  EINVAL);
    finish_backup_thread(thread);
    free(src);
    free(dst);
    return 0;
}
