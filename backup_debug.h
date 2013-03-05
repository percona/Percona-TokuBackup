/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#ifndef BACKUP_DEBUG_H
#define BACKUP_DEBUG_H

#ifndef DEBUG_HOTBACKUP
#define DEBUG_HOTBACKUP 0
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
} // End of namespace.
#endif // End of header guardian.
