/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

//
// Unit tests for file descriptor map.
//

#include "file_descriptor_map.h"
#include <stdio.h>

class file_descriptor_map_unit_test {
public:
    int size(file_descriptor_map *map) {
        return map->m_map.size();
    }
};

int count = 0;

void report(bool passed)
{
    if (passed) {
        printf("Test %d passed.\n", count++);    
    } else {
        printf("Test %d FAILED!\n", count++);
    }
}

bool get_from_empty_map()
{
    bool result = false;
    file_descriptor_map map;
    file_description* desc = NULL;
    desc = map.get(0);
    if (desc) goto exit;
    desc = map.get(1);
    if (desc) goto exit;
    desc = map.get(2);
    if (desc) goto exit;
    result = true;
exit:
    return result;
}


bool add_and_remove()
{
    bool result = false;
    file_descriptor_map_unit_test unit;
    file_descriptor_map map;
    file_description *desc = NULL;
    map.put(1);
    int size = unit.size(&map);
    if (size != 2) goto exit;
    result = true;
exit:
    return result;
}
int main(int argc, const char* argv[])
{
    report(get_from_empty_map());
    report(add_and_remove());
    
    return 0;
}
