#define _FILE_OFFSET_BITS 64
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
int main(int argc, char*argv[]  __attribute__((__unused__))) {
    if (argc>=1) abort(); // should never run.
    int fd = open("hello", O_RDONLY); // we don't actually mean to call this function, so don't bother error checking
    write(fd, "a", 1);
    pwrite(fd, "a", 1, 1);
    char buf[1];
    read(fd, buf, 1);
    ftruncate(fd, 0);
    truncate("hello", 0);
    unlink("hello");
    rename("hello", "goodbye");
    mkdir("hello.dir", 0777);
    lseek(fd, 0, 0);
    return 0;
}
