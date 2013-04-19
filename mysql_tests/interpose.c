#include <fcntl.h>
#include <unistd.h>

void ignorev(const void *i __attribute__((unused))) {}
void ignorei(long i __attribute__((unused))) {}


int open64(const char *fname, int flags, ...) {
    ignorev(fname); // use this instead of attribute so that the prototype stays clean.
    ignorei(flags);
    return -1;
}

ssize_t write(int fd, const void *buf, size_t size) {
    ignorei(fd);
    ignorev(buf);
    ignorei(size);
    return -1;
}

ssize_t read(int fd, void *buf, size_t nbyte) {
    ignorei(fd);
    ignorev(buf);
    ignorei(nbyte);
    return -1;    
}

ssize_t pwrite64(int fd, const void *buf, size_t nbyte, off_t offset) {
    ignorei(fd);
    ignorev(buf);
    ignorei(nbyte);
    ignorei(offset);
    return -1;
}
    
int unlink(const char *path) {
    ignorev(path);
    return -1;
}

int rename(const char *oldpath, const char *newpath) {
    ignorev(oldpath);
    ignorev(newpath);
    return -1;
}
        
int mkdir(const char *pathname, mode_t mode) {
    ignorev(pathname);
    ignorei(mode);
    return -1;
}

int truncate64(const char *path, off_t length) {
    ignorev(path);
    ignorei(length);
    return -1;
}

int ftruncate64(int fd, off_t length) {
    ignorei(fd);
    ignorei(length);
    return -1;
}

off_t lseek64(int fd, off_t offset, int whence) {
    ignorei(fd);
    ignorei(offset);
    ignorei(whence);
    return -1;
}

