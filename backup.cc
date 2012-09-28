#include <stdio.h>

// Capture open calls.
int open(const char* file, int oflag, ...)
{
    int result;
    result = 0;
    printf("open called.\n");
    return result;
}

//
int close(int fd)
{
    int result;
    result = 0;
    printf("close called.\n");
    return result;
}

//
ssize_t write(int fd, const void *buf, size_t nbyte)
{
    ssize_t result;
    printf("write called.\n");
    return result;
}

//
int rename (const char *oldpath, const char *newpath)
{
    int result = 0;
    printf("rename called.\n");
    return result;
}

