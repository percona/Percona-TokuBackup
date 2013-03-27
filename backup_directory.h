/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#ifndef BACKUP_DIRECTORY_H
#define BACKUP_DIRECTORY_H

#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include "file_description.h"
#include "file_descriptor_map.h"
#include "backup_copier.h"
#include "backup_callbacks.h"

#include <pthread.h>
#include <vector>
#include <pthread.h>


//////////////////////////////////////////////////////////////////////////////
//
class backup_session_control
{
public:
    virtual void abort_backup() {};
    virtual void throttle(int amount) {amount++;};
};

//////////////////////////////////////////////////////////////////////////////
//
class backup_session
{
public:
    backup_session(const char *source, const char *dest, backup_callbacks *call, int *errnum); // returns a nonzero in *errnum if an error callback has occured.
    ~backup_session();
    int do_copy();
    int directories_set(backup_callbacks*);
    bool is_prefix(const char *file);
    char* translate_prefix(const char *file);
    // TODO: Add error code and string to abort().
    void abort() {};
    
    // Capture interface.
    char * capture_open(const char *file);
    char * capture_create(const char *file);
    void capture_mkdir(const char *pathname);
protected:

private:
    const char *m_source_dir;
    const char *m_dest_dir;
    backup_copier m_copier;
};


//////////////////////////////////////////////////////////////////////////////
//
//class backup_directory
//{
//private:
//    const char *m_source_dir;
//    const char *m_dest_dir;
//    backup_copier m_copier;
//    pthread_t m_thread;
//public:
//    backup_directory();
//    int open_path(const char *file_path);
//    
//    char* translate_prefix(const char *file);
//    int set_directories(const char *source, const char *dest, backup_poll_fun_t poll_fun, void *poll_extra, backup_error_fun_t error_fun, void *error_extra) __attribute__((warn_unused_result));
//    void create_subdirectories(const char *file);
//};

#endif // End of header guardian.
