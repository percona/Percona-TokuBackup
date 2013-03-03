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
    const char *m_backup_name;
    const char *m_full_source_name;

    // NOTE: in the 'real' application, we may use another way to name
    // the destination file, like its name.
public:
    file_description();
    void prepare_for_backup(const char *name);
    void set_full_source_name(const char *name);
    const char * get_full_source_name(void);
    void open();
    void create();
    void write(const void *buf, size_t nbyte);
    void pwrite(const void *buf, size_t nbyte, off_t offset);
    void seek(size_t nbyte, int whence);
    void close();
    
    bool m_in_source_dir;
};

#endif // end of header guardian.
