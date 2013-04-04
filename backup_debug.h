/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#ifndef BACKUP_DEBUG_H
#define BACKUP_DEBUG_H

#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#ifndef DEBUG_HOTBACKUP
#define DEBUG_HOTBACKUP 1
#endif

namespace HotBackup {

// Debug flag for backup_copier debugging.
const int CPY_DBG = 0;
// Debug flag just for the backup manager.
const int MGR_DBG = 0;
// Debug flag for the file descriptor map.
const int MAP_DBG = 0;

// Components:
// COPY
const int COPY_FLAG = 0x01;
// CAPTURE
const int CAPUTRE_FLAG = 0x02;
// INTERPOSE
const int INTERPOSE_FLAG = 0x04;

void CopyTrace(const char *s, const char *arg);
void CopyWarn(const char *s, const char *arg);
void CopyError(const char *s, const char *arg);
void CaptureTrace(const char *s, const char *arg);
void CaptureTrace(const char *s, const int arg);
void CaptureWarn(const char *s, const char *arg);
void CaptureError(const char *s, const char *arg);
void CaptureError(const char *s, const int arg);
void InterposeTrace(const char *s, const char *arg);
void InterposeTrace(const char *s, const int arg);
void InterposeWarn(const char *s, const char *arg);
void InterposeError(const char *s, const char *arg);

// Pause Points:
//
// Pause Point Flags:
//
const int COPIER_BEFORE_READ                = 0x01;
const int COPIER_AFTER_READ_BEFORE_WRITE    = 0x02;
const int COPIER_AFTER_WRITE                = 0x04;
const int MANAGER_IN_PREPARE                = 0x08;
const int MANAGER_IN_DISABLE                = 0x10;

bool should_pause(int);
void set_pause(int);

} // End of namespace.



#endif // End of header guardian.
