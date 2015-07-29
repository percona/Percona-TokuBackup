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

#ifndef BACKUP_DEBUG_H
#define BACKUP_DEBUG_H

#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#ifndef DEBUG_HOTBACKUP
#define DEBUG_HOTBACKUP 0
#endif

#ifndef PAUSE_POINTS_ON
#define PAUSE_POINTS_ON 1
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

void CopyTrace(const char *s, const char *arg) throw();
void CopyWarn(const char *s, const char *arg) throw();
void CopyError(const char *s, const char *arg) throw();
void CaptureTrace(const char *s, const char *arg) throw();
void CaptureTrace(const char *s, const int arg) throw();
void CaptureWarn(const char *s, const char *arg) throw();
void CaptureError(const char *s, const char *arg) throw();
void CaptureError(const char *s, const int arg) throw();
void InterposeTrace(const char *s, const char *arg) throw();
void InterposeTrace(const char *s, const int arg) throw();
void InterposeWarn(const char *s, const char *arg) throw();
void InterposeError(const char *s, const char *arg) throw();

// Pause Points:
//
// Pause Point Flags:
//
const int COPIER_BEFORE_READ                = 0x01;
const int COPIER_AFTER_READ_BEFORE_WRITE    = 0x02;
const int COPIER_AFTER_WRITE                = 0x04;
const int MANAGER_IN_PREPARE                = 0x08;
const int MANAGER_IN_DISABLE                = 0x10;
const int COPIER_AFTER_OPEN_SOURCE          = 0x20;
const int OPEN_DESTINATION_FILE             = 0x40;
const int CAPTURE_OPEN                      = 0x80;

bool should_pause(int) throw();
void toggle_pause_point(int) throw();

} // End of namespace.



#endif // End of header guardian.
