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

#include "backup.h"
#include <errno.h>
#include <stdio.h>

extern "C" int tokubackup_create_backup(const char *source_dirs[]  __attribute__((unused)),
                                        const char *dest_dirs[]    __attribute__((unused)),
                                        int dir_count              __attribute__((unused)),
                                        backup_poll_fun_t poll_fun __attribute__((unused)),
                                        void *poll_extra           __attribute__((unused)),
                                        backup_error_fun_t error_fun,
                                        void *error_extra) {
    error_fun(ENOSYS, "Sorry, backup is not implemented", error_extra);
    return ENOSYS;
}

extern "C" void tokubackup_throttle_backup(unsigned long bytes_per_second __attribute__((unused))) {
    fprintf(stderr, "Sorry, backup is not implemented\n");
}

const char tokubackup_sql_suffix[] = "";
