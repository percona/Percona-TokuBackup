/* Inject enospc. */

#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "backup_test_helpers.h"
#include "backup_internal.h"
#include "real_syscalls.h"

static pwrite_fun_t original_pwrite;

static ssize_t my_pwrite(int fd, const void *buf, size_t nbyte, off_t offset) {
    fprintf(stderr, "Doing write on fd=%d\n", fd); // ok to do a write, since we aren't further interposing writes in this test.
    return original_pwrite(fd, buf, nbyte, offset);
}

int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    setup_source();
    setup_dirs();
    setup_destination();

    char *src = get_src();

    original_pwrite = register_pwrite(my_pwrite);

    backup_set_keep_capturing(true);
    pthread_t thread;

    int fd = openf(O_RDWR|O_CREAT, 0777, "%s/my.data", src);
    assert(fd>=0);
    fprintf(stderr, "fd=%d\n", fd);
    usleep(10000);
    {
        ssize_t r = pwrite(fd, "hello", 5, 10);
        assert(r==5);
    }
    {
        int r = close(fd);
        assert(r==0);
    }

    start_backup_thread(&thread);

    backup_set_keep_capturing(false);
    finish_backup_thread(thread);

    free(src);

    return 0;
}
