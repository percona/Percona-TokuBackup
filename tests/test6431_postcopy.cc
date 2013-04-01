/* Inject enospc. */

#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "backup_test_helpers.h"
#include "backup_internal.h"
#include "real_syscalls.h"

int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    setup_source();
    setup_dirs();
    setup_destination();

    char *src = get_src();

    backup_set_keep_capturing(true);
    pthread_t thread;

    const int n_fds = 20;
    int fds[n_fds];
    for (int i=0; i<n_fds; i++) {
        fds[i] = openf(O_RDWR|O_CREAT, 0777, "%s/my%d.data", src, i);
        assert(fds[i]>=0);
    }
    
    start_backup_thread(&thread);

    backup_set_keep_capturing(false);
    usleep(1000);
    printf("said to keep capturing\n");

    int n_fds_left = n_fds;
    int count = 0;
    while (n_fds_left) {
        int idx = random()%n_fds_left;
        int fd = fds[idx];
        fds[idx] = fds[n_fds_left-1];
        n_fds_left--;

        char data[100];
        int len = snprintf(data, sizeof(data), "data_order %d\n", count);
        {
            ssize_t r = write(fd, data, len);
            assert(r==len);
        }
        {
            int r = close(fd);
            assert(r==0);
        }
        count++;
        
    }
    
    finish_backup_thread(thread);

    free(src);

    return 0;
}
