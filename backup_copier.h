/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#ifndef BACKUP_COPIER
#define BACKUP_COPIER

#include <vector>

class backup_copier {
    const char *m_source;
    const char *m_dest;
    std::vector<char *> m_todo;
public:
    backup_copier();
    void set_directories(const char *source, const char *dest);
    void start_copy();
    void copy_file(const char *file);
    void copy_path(const char *source, const char* dest, const char *file);
};

#endif // End of header guardian.
