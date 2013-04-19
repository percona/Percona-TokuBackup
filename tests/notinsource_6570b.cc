#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id: unlink_during_copy_test6515.cc 55458 2013-04-13 21:44:50Z bkuszmaul $"

// Test doing operations that are in a directory that's not backed up.

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "backup_test_helpers.h"

char *not_src;

void fileops(int fd, const char *string) {
    assert(strlen(string)>30); // this test needs a certain amount of string...
    int len = strlen(string);
    {
        ssize_t r = write(fd, string, len);
        assert(r==(ssize_t)len);
    }
    {
        off_t r = lseek(fd, -1, SEEK_CUR);
        assert(r==(ssize_t)(len-1));
    }
    {
        off_t r = lseek(fd, -2, SEEK_END);
        assert(r==(ssize_t)(len-2));
    }
    {
        off_t r = lseek(fd, 1, SEEK_SET);
        assert(r==1);
    }
    {
        off_t r = lseek(fd, 0, SEEK_SET);
        assert(r==0);
    }
    {
        char buf[len];
        ssize_t r = read(fd, buf, len);
        assert(r==(ssize_t)len);
        assert(memcmp(buf, string, len)==0);
    }
    {
        char buf[10];
        ssize_t r = pread(fd, buf, 10, 10);
        assert(r==10);
        assert(memcmp(buf, string+10, 10)==0);
    }
    // check that the lseek is in the right place still after the pread
    {
        off_t r = lseek(fd, 0, SEEK_CUR);
        assert(r==len);
    }
    {
        char data[] = "data";
        ssize_t r = pwrite(fd, data, sizeof(data)-1, 10);
        assert(r==sizeof(data)-1);
    }
    {
        off_t r = lseek(fd, 0, SEEK_CUR);
        assert(r==len);
    }
    {
        char buf[len];
        ssize_t r = pread(fd, buf, len, 0);
        assert(r==len);
        assert(memcmp(buf,      string, 10)==0);
        assert(memcmp(&buf[10], "data", 4) ==0);
        assert(memcmp(&buf[14], string+14, len-14)==0);
    }
    {
        off_t r = lseek(fd, 0, SEEK_CUR);
        assert(r==len);
    }
    {
        int r = ftruncate(fd, 10);
        assert(r==0);
    }
    {
        off_t r = lseek(fd, 0, SEEK_END);
        assert(r==10);
    }
}

void exercise(void) {
    size_t nlen = strlen(not_src)+10;
    // open, fileops(read,write, etc) close, unlink
    {
        char filea_name[nlen];
        size_t actual_len = snprintf(filea_name, nlen, "%s/filea", not_src);
        assert(actual_len < nlen);
        int fda = open(filea_name, O_RDWR|O_CREAT, 0777);
        assert(fda>=0);
        fileops(fda, filea_name);
        {
            int r = close(fda);
            assert(r==0);
        }
        {
            int r = unlink(filea_name);
            assert(r==0);
        }
    }
    // open, unlink, fileops(read,write, etc), close.  The key is that unlink was done first.
    {
        char filea_name[nlen];
        size_t actual_len = snprintf(filea_name, nlen, "%s/filea", not_src);
        assert(actual_len < nlen);
        int fda = open(filea_name, O_RDWR|O_CREAT, 0777);
        assert(fda>=0);
        {
            int r = unlink(filea_name);
            assert(r==0);
        }
        fileops(fda, filea_name);
        {
            int r = close(fda);
            assert(r==0);
        }
    }
    {
        char dirname[nlen];
        size_t actual_len = snprintf(dirname, nlen, "%s/newdir", not_src);
        assert(actual_len < nlen);
        {
            int r = mkdir(dirname, 0777);
            if (r!=0) fprintf(stderr, "mkdir(%s) failed r=%d errno=%d (%s)\n", dirname, r, errno, strerror(errno));
            assert(r==0);
        }
        {
            int r = rmdir(dirname);
            assert(r==0);
        }
    }
    // open, write, rename, close2
    return; // the following is broken
    {
        char filea_name[nlen];
        char fileb_name[nlen];
        size_t actual_len  = snprintf(filea_name, nlen, "%s/filea", not_src);
        size_t actual_lenb = snprintf(fileb_name, nlen, "%s/fileb", not_src);
        assert(actual_len == actual_lenb);
        int fda = open(filea_name, O_RDWR|O_CREAT, 0777);
        assert(fda>=0);
        {
            ssize_t r = write(fda, filea_name, actual_len);
            assert(r==(ssize_t)actual_len);
        }
        {
            int r = rename(filea_name, fileb_name);
            assert(r==0);
        }
        {
            int r = close(fda);
            assert(r==0);
        }
    }
}

int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    char *src = get_src();
    size_t slen = strlen(src);
    const char N_EXTRA_BYTES = 10;
    not_src = (char*)malloc(slen+N_EXTRA_BYTES);
    strncpy(not_src, src, slen+N_EXTRA_BYTES);
    const char go_back_n_bytes = 7;
    printf("backed up =%s\n",not_src+slen-go_back_n_bytes);
    assert(0==strcmp(not_src+slen-go_back_n_bytes, ".source"));
    size_t n_written = snprintf(not_src+slen - go_back_n_bytes, N_EXTRA_BYTES+go_back_n_bytes, ".notsource");
    assert(n_written< N_EXTRA_BYTES+go_back_n_bytes);
    {
        int r = systemf("rm -rf %s", not_src);
        assert(r==0);
    }
    {
        int r = mkdir(not_src, 0777);
        assert(r==0);
    }
    setup_source();
    setup_destination();

    int fd = openf(O_WRONLY|O_CREAT, 0777, "%s/data", src);
    assert(fd>=0);
    {
        const int bufsize=1024;
        const int nbufs  =1024;
        char buf[bufsize];
        for (int i=0; i<bufsize; i++) {
            buf[i]=i%256;
        }
        for (int i=0; i<nbufs; i++) {
            ssize_t r = write(fd, buf, bufsize);
            assert(r==bufsize);
        }
    }
    tokubackup_throttle_backup(1L<<18); // quarter megabyte per second, so that's 4 seconds.
    pthread_t thread;
    start_backup_thread(&thread);
    while (!backup_thread_is_done()) {
        exercise();
    }
    exercise();
    finish_backup_thread(thread);
    // And do two more.
    exercise();
    exercise();
    free(src);
    return 0;
}
