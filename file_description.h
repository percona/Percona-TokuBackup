#ifndef FILE_DESCRIPTION_H
#define FILE_DESCRIPTION_H

#include <sys/types.h>
#include <vector>

class file_description {
public:
    file_description();
    void print();
    void open();
    void write(const void *buf, size_t nbyte);
    void pwrite(const void *buf, size_t nbyte, off_t offset);
    void close();
public:
    int refcount;
    off_t offset;
    std::vector<int> fds; // which fds refer to this file description.
    int fd_in_dest_space;   // what is the fd in the destination space?
    char *name;
    // NOTE: in the 'real' application, we may use another way to name
    // the destination file, like its name.
};

#endif // end of header guardian.
