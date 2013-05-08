/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id: destination_file.cc 55911 2013-04-26 10:24:25Z bkuszmaul $"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "destination_file.h"
#include "glassbox.h"
#include "manager.h"
#include "real_syscalls.h"

///////////////////////////////////////////////////////////////////////////////
//
destination_file::destination_file(const int opened_fd, const char * full_path)
    : m_fd(opened_fd), m_path(full_path)
{};

///////////////////////////////////////////////////////////////////////////////
//
destination_file::~destination_file()
{
    if (m_path != NULL) {
        free((void*)m_path);
    }
}

///////////////////////////////////////////////////////////////////////////////
//
int destination_file::close(void) const
{
    // TODO: #6544 Check refcount, if it's zero we REALLY have to close
    // the file.  Otherwise, if there are any references left, 
    // we can only decrement the refcount; other file descriptors
    // are still open in the main application.
    int r = call_real_close(m_fd);
    if (r == -1) {
        r = errno;
        the_manager.backup_error(r, "Trying to close a backup file (fd=%d)", m_fd);
    }

    return r;
}

///////////////////////////////////////////////////////////////////////////////
//
int destination_file::pwrite(const void *buf, size_t nbyte, off_t offset) const
{
    // Get the data written out, or do 
    while (nbyte > 0) {
        ssize_t wr = call_real_pwrite(m_fd, buf, nbyte, offset);
        if (wr == -1) {
            int r = errno;
            the_manager.backup_error(r, "Failed to pwrite backup file at %s:%d", __FILE__, __LINE__);
            return r;
        }

        if (wr == 0) {
            // Can this happen?  Don't see how.  If it does happen, treat it as an error.
            int r = -1; // Unknown error
            the_manager.backup_error(-1, "pwrite inexplicably returned zero at %s:%d", __FILE__, __LINE__);
            return r;
        }

        nbyte -= wr;
        offset += wr;
    }

    return 0;
}

///////////////////////////////////////////////////////////////////////////////
//
int destination_file::truncate(off_t length) const
{
    int r = 0;
    r = call_real_ftruncate(m_fd, length);
    if (r != 0) {
        r = errno;
        the_manager.backup_error(r, "Truncating backup file failed at %s:%d", __FILE__, __LINE__);
    }

    return r;
}

///////////////////////////////////////////////////////////////////////////////
//
int destination_file::unlink(void) const
{
    int r = call_real_unlink(m_path);
    if (r != 0) {
        r = errno;
        the_manager.backup_error(r, "Unlink of backup file failed.");
    }

    return r;
}

///////////////////////////////////////////////////////////////////////////////
//
int destination_file::rename(const char *new_path)
{
    int r = 0;
    char * new_destination_path = strdup(new_path);
    if (new_destination_path == NULL) {
        r = errno;
        return r;
    }

    r = call_real_rename(m_path, new_destination_path);
    if (r != 0) {
        r = errno;
        // Ignore the error where the copier hasn't yet copied the
        // original file.
        if (r != ENOENT) {
            free((void*) new_destination_path);
            the_manager.backup_error(r, "Rename failed on backup file.");
            return r;
        }
    }

    free((void*)m_path);
    m_path = new_destination_path;
    return r;
}

///////////////////////////////////////////////////////////////////////////////
//
int destination_file::get_fd(void) const
{
    return m_fd;
}

///////////////////////////////////////////////////////////////////////////////
//
const char * destination_file::get_path(void) const
{
    return m_path;
}
