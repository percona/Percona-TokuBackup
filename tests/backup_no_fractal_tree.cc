#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "backup_test_helpers.h"
#include "backup_internal.h"

char *src, *dst;

#define FIRSTBYTES "first bytes\n"
#define MOREBYTES  "more bytes\n"

int test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    setup_destination();
    setup_source();
    src = get_src();
    dst = get_dst();

    int fd0 = openf(O_CREAT | O_WRONLY, S_IRWXU | S_IRWXG | S_IRWXO, "%s/file0", src);
    assert(fd0>=0);
    {
        ssize_t r = write(fd0, FIRSTBYTES, sizeof(FIRSTBYTES)-1);
        assert(r==sizeof(FIRSTBYTES)-1);
    }
    backup_set_keep_capturing(true);
    pthread_t thread;
    start_backup_thread(&thread);
    usleep(100000);
    {
        ssize_t r = write(fd0, MOREBYTES, sizeof(MOREBYTES)-1);
        assert(r==sizeof(MOREBYTES)-1);
    }
    backup_set_keep_capturing(false);    
    finish_backup_thread(thread);
    {
        int status = systemf("diff -r %s %s", src, dst);
        assert(status!=-1);
        assert(WIFEXITED(status));
        assert(WEXITSTATUS(status)==0);
    }
    free(src);
    free(dst);
    return 0;
}
