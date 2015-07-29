/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
/*======
This file is part of Percona TokuBackup.

Copyright (c) 2006, 2015, Percona and/or its affiliates. All rights reserved.

    Percona TokuBackup is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License, version 2,
    as published by the Free Software Foundation.

     Percona TokuBackup is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Percona TokuBackup.  If not, see <http://www.gnu.org/licenses/>.

----------------------------------------

    Percona TokuBackup is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License, version 3,
    as published by the Free Software Foundation.

    Percona TokuBackup is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with Percona TokuBackup.  If not, see <http://www.gnu.org/licenses/>.
======= */

#ident "$Id$"

#include "backup_debug.h"
#include "backup_helgrind.h"
#include <stdio.h>

namespace HotBackup {

// TODO: Set each flag with getenv...
int BACKUP_TRACE_FLAGS = 0x0F;
int BACKUP_WARN_FLAGS = 0x0F;
int BACKUP_ERROR_FLAGS = 0x0F;

void CopyTrace(const char *s, const char *arg) throw() {
    if(BACKUP_TRACE_FLAGS & COPY_FLAG) {
        printf("TRACE: <COPY> %s %s\n", s, arg);
    }
}

void CopyWarn(const char *s, const char *arg) throw() {
    if(BACKUP_WARN_FLAGS & COPY_FLAG) {
        printf("WARN: <COPY> %s %s\n", s, arg);
    }
}

void CopyError(const char *s, const char *arg) throw() {
    if(BACKUP_ERROR_FLAGS & COPY_FLAG) {
        printf("ERROR: <COPY> %s %s\n", s, arg);
    }
}

void CaptureTrace(const char *s, const char *arg) throw() {
    if(BACKUP_TRACE_FLAGS & CAPUTRE_FLAG) {
        printf("TRACE: <CAPTURE> %s %s\n", s, arg);
    }
}

void CaptureTrace(const char *s, const int arg) throw() {
    if(BACKUP_TRACE_FLAGS & CAPUTRE_FLAG) {
        printf("TRACE: <CAPTURE> %s %d\n", s, arg);
    }
}

void CaptureWarn(const char *s, const char *arg) throw() {
    if(BACKUP_WARN_FLAGS & CAPUTRE_FLAG) {
        printf("WARN: <CAPTURE> %s %s\n", s, arg);
    }
}

void CaptureError(const char *s, const char *arg) throw() {
    if(BACKUP_ERROR_FLAGS & CAPUTRE_FLAG) {
        printf("ERROR: <CAPTURE> %s %s\n", s, arg);
    }
}

void CaptureError(const char *s, const int arg) throw() {
    if(BACKUP_ERROR_FLAGS & CAPUTRE_FLAG) {
        printf("ERROR: <CAPTURE> %s %d\n", s, arg);
    }
}

void InterposeTrace(const char *s, const char *arg) throw() {
    if(BACKUP_TRACE_FLAGS & INTERPOSE_FLAG) {
        printf("TRACE: <INTERPOSE> %s %s\n", s, arg);
    }
}

void InterposeTrace(const char *s, const int arg) throw() {
    if(BACKUP_TRACE_FLAGS & INTERPOSE_FLAG) {
        printf("TRACE: <INTERPOSE> %s %d\n", s, arg);
    }
}

void InterposeWarn(const char *s, const char *arg) throw() {
    if(BACKUP_WARN_FLAGS & INTERPOSE_FLAG) {
        printf("WARN: <INTERPOSE> %s %s\n", s, arg);
    }
}

void InterposeError(const char *s, const char *arg) throw() {
    if(BACKUP_ERROR_FLAGS & INTERPOSE_FLAG) {
        printf("ERROR: <INTERPOSE> %s %s\n", s, arg);
    }
}

static int PAUSE_POINTS = 0x00;

bool should_pause(int flag) throw() {
    bool result = false;
    switch (flag) {
        case COPIER_BEFORE_READ:
            result = COPIER_BEFORE_READ & PAUSE_POINTS;
            break;
        case COPIER_AFTER_READ_BEFORE_WRITE:
            result = COPIER_AFTER_READ_BEFORE_WRITE & PAUSE_POINTS;
            break;
        case COPIER_AFTER_WRITE:
            result = COPIER_AFTER_WRITE & PAUSE_POINTS;
            break;
        case MANAGER_IN_PREPARE:
            result = MANAGER_IN_PREPARE;
            break;
        case MANAGER_IN_DISABLE:
            result = MANAGER_IN_DISABLE;
            break;
        case COPIER_AFTER_OPEN_SOURCE:
            result = COPIER_AFTER_OPEN_SOURCE & PAUSE_POINTS;
            break;
        case OPEN_DESTINATION_FILE:
            result = OPEN_DESTINATION_FILE & PAUSE_POINTS;
            break;
        case CAPTURE_OPEN:
            result = CAPTURE_OPEN & PAUSE_POINTS;
            break;
        default:
            break;
    }

    return result;
}

void toggle_pause_point(int flag) throw() {
    TOKUBACKUP_VALGRIND_HG_DISABLE_CHECKING(&PAUSE_POINTS, sizeof(PAUSE_POINTS));
    PAUSE_POINTS = PAUSE_POINTS ^ flag;
}

} // End of namespace.

