/* Inject enospc. */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "backup_test_helpers.h"
#include "backup_internal.h"
#include "real_syscalls.h"

static pwrite_fun_t original_pwrite;

static ssize_t my_pwrite(int fd, const void *buf, size_t nbyte, off_t offset) {
    fprintf(stderr, "Doing pwrite on fd=%d\n", fd); // ok to do a write, since we aren't further interposing writes in this test.
    return original_pwrite(fd, buf, nbyte, offset);
}

static write_fun_t  original_write;
static ssize_t my_write(int fd, const void *buf, size_t nbyte) {
    fprintf(stderr, "Doing write(%d, %p, %ld)\n", fd, buf, nbyte); // ok to do a write, since we aren't further interposing writes in this test.
    errno = ENOSPC;
    return -1;
    //return original_write(fd, buf, nbyte);
}

int ercount=0;
static void my_error_fun(int e, const char *s, void *ignore) {
    assert(ignore==NULL);
    ercount++;
    fprintf(stderr, "Got error %d (I expected errno=%d) (%s)\n", e, ENOSPC, s);
}
    

int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    setup_source();
    setup_dirs();
    setup_destination();

    char *src = get_src();

    original_pwrite = register_pwrite(my_pwrite);
    original_write  = register_write(my_write);

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

    start_backup_thread_with_funs(&thread,
                                  get_src(), get_dst(),
                                  simple_poll_fun, NULL,
                                  my_error_fun, NULL,
                                  ENOSPC);

    backup_set_keep_capturing(false);
    finish_backup_thread(thread);

    free(src);

    return 0;
}
