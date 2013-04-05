/* Inject enospc. */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include "backup_test_helpers.h"
#include "backup_internal.h"

static char *src, *dst;

static void testit(void) {

    setup_source();
    setup_dirs();
    setup_destination();

    pthread_t thread;

    int fd = openf(O_WRONLY|O_CREAT, 0777, "%s/my.data", src);
    assert(fd>=0);
    fprintf(stderr, "fd=%d\n", fd);

    backup_set_keep_capturing(true);
    start_backup_thread_with_funs(&thread,
                                  get_src(), get_dst(),
                                  dummy_poll, NULL,
                                  dummy_error, NULL,
                                  0);
    fprintf(stderr, "The backup is supposedly capturing\n");
    while (!backup_is_capturing()) sched_yield(); // wait for capturing to start
    while (!backup_done_copying()) sched_yield(); // then wait for copying to finish
    // Now capturing is still running.
    {
        ssize_t r = write(fd, "hello", 5);
        assert(r==5);
    }
    {
        char buf[20];
        ssize_t r = read(fd, buf, 20);
        printf("r=%ld\n", r);
        assert(r==-1);
    }
    {
        ssize_t r = write(fd, "good", 4);
        assert(r==4);
    }
    fprintf(stderr,"About to start copying\n");
    backup_set_keep_capturing(false);
    {
        int r = close(fd);
        assert(r==0);
    }

    backup_set_keep_capturing(false);
    finish_backup_thread(thread);
    {
        int fd2 = openf(O_RDWR, 0, "%s/my.data", dst);
        assert(fd2>=0);
        {
            struct stat buf;
            int r = fstat(fd2, &buf);
            assert(r==0);
            if (buf.st_size!=9) fprintf(stderr, "Expect file size = 9, it is %ld\n", buf.st_size);
            assert(buf.st_size==9);
        }
        {
            int r = close(fd2);
            assert(r==0);
        }
    }
}


int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    src = get_src();
    dst = get_dst();

    testit();
    
    free(src);
    free(dst);
    return 0;
}
