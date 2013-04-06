/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id: file_descriptor_map.h 54931 2013-03-29 19:51:23Z bkuszmaul $"

#include <unistd.h>
#include <pthread.h>

#include "source_file.h"

////////////////////////////////////////////////////////
//
source_file::source_file(const char * path) : m_full_path(path), m_next(NULL) 
{
};

int source_file::init(void)
{
    int r = pthread_mutex_init(&m_mutex, NULL);
    return r;
}

////////////////////////////////////////////////////////
//
const char * const source_file::name(void)
{
    return m_full_path;
}

////////////////////////////////////////////////////////
//
source_file * source_file::next(void)
{
    return m_next;
}

////////////////////////////////////////////////////////
//
void source_file::set_next(source_file *next)
{
    m_next = next;
}

////////////////////////////////////////////////////////
//
void source_file::lock_range(void)
{
    pthread_mutex_lock(&m_mutex);
}

////////////////////////////////////////////////////////
//
void source_file::wait_on_range(void)
{
    
}

////////////////////////////////////////////////////////
//
void source_file::signal_range(void)
{

}

////////////////////////////////////////////////////////
//
void source_file::unlock_range(void)
{
    pthread_mutex_unlock(&m_mutex);
}

////////////////////////////////////////////////////////
//
void source_file::add_reference(void)
{
    ++m_reference_count;
}

////////////////////////////////////////////////////////
//
void source_file::remove_reference(void)
{
    if (m_reference_count != 0) {
        --m_reference_count;
    }
}

////////////////////////////////////////////////////////
//
unsigned int source_file::get_reference_count(void)
{
    return m_reference_count;
}
