/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#include "backup_debug.h"
#include <stdio.h>

namespace HotBackup {

// TODO: Set each flag with getenv...
int BACKUP_TRACE_FLAGS = 0x0F;
int BACKUP_WARN_FLAGS = 0x0F;
int BACKUP_ERROR_FLAGS = 0x0F;

void CopyTrace(const char *s, const char *arg) 
{
    if(BACKUP_TRACE_FLAGS & COPY_FLAG) {
        printf("TRACE: <COPY> %s %s\n", s, arg);
    }
}

void CopyWarn(const char *s, const char *arg)
{
    if(BACKUP_WARN_FLAGS & COPY_FLAG) {
        printf("WARN: <COPY> %s %s\n", s, arg);
    }
}

void CopyError(const char *s, const char *arg)
{
    if(BACKUP_ERROR_FLAGS & COPY_FLAG) {
        printf("ERROR: <COPY> %s %s\n", s, arg);
    }
}

void CaptureTrace(const char *s, const char *arg) 
{
    if(BACKUP_TRACE_FLAGS & CAPUTRE_FLAG) {
        printf("TRACE: <CAPTURE> %s %s\n", s, arg);
    }
}

void CaptureTrace(const char *s, const int arg)
{
    if(BACKUP_TRACE_FLAGS & CAPUTRE_FLAG) {
        printf("TRACE: <CAPTURE> %s %d\n", s, arg);
    }
}

void CaptureWarn(const char *s, const char *arg)
{
    if(BACKUP_WARN_FLAGS & CAPUTRE_FLAG) {
        printf("WARN: <CAPTURE> %s %s\n", s, arg);
    }
}

void CaptureError(const char *s, const char *arg)
{
    if(BACKUP_ERROR_FLAGS & CAPUTRE_FLAG) {
        printf("ERROR: <CAPTURE> %s %s\n", s, arg);
    }
}

void CaptureError(const char *s, const int arg)
{
    if(BACKUP_ERROR_FLAGS & CAPUTRE_FLAG) {
        printf("ERROR: <CAPTURE> %s %d\n", s, arg);
    }
}

void InterposeTrace(const char *s, const char *arg) 
{
    if(BACKUP_TRACE_FLAGS & INTERPOSE_FLAG) {
        printf("TRACE: <INTERPOSE> %s %s\n", s, arg);
    }
}

void InterposeTrace(const char *s, const int arg) 
{
    if(BACKUP_TRACE_FLAGS & INTERPOSE_FLAG) {
        printf("TRACE: <INTERPOSE> %s %d\n", s, arg);
    }
}

void InterposeWarn(const char *s, const char *arg)
{
    if(BACKUP_WARN_FLAGS & INTERPOSE_FLAG) {
        printf("WARN: <INTERPOSE> %s %s\n", s, arg);
    }
}

void InterposeError(const char *s, const char *arg)
{
    if(BACKUP_ERROR_FLAGS & INTERPOSE_FLAG) {
        printf("ERROR: <INTERPOSE> %s %s\n", s, arg);
    }
}

}

