#ifndef FILE_DESCRIPTION_H
#define FILE_DESCRIPTION_H

#include <sys/types.h>
#include <vector>

class file_description {
private:
    int m_refcount;
    off_t m_offset;
    std::vector<int> m_fds; // which fds refer to this file description.
    int m_fd_in_dest_space;   // what is the fd in the destination space?
    char *m_name;
    // NOTE: in the 'real' application, we may use another way to name
    // the destination file, like its name.
public:
    file_description();
    void set_name(char *name);
    void print();
    void open();
    void create();
    void write(const void *buf, size_t nbyte);
    void pwrite(const void *buf, size_t nbyte, off_t offset);
    void seek(size_t nbyte);
    void close();
};

#endif // end of header guardian.
