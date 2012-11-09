/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#ifndef FILE_DESCRIPTOR_MAP_H
#define FILE_DESCRIPTOR_MAP_H
#endif // End of header guardian.

#include <vector>
#include "file_description.h"

class file_descriptor_map
{
public:
    file_descriptor_map();
    file_description* get(int fd);
    void put(int fd);
    void erase(int fd);
private:
    std::vector<file_description *> m_map;
    void grow_array(int fd);
};