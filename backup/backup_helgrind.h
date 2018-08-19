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
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."

#ifndef BACKUP_HELGRIND_H
#define BACKUP_HELGRIND_H

#if defined(BACKUP_USE_VALGRIND)
  #include <valgrind/helgrind.h>
  #define TOKUBACKUP_VALGRIND_HG_DISABLE_CHECKING(x, y) VALGRIND_HG_DISABLE_CHECKING(x, y)
  #define TOKUBACKUP_VALGRIND_HG_ENABLE_CHECKING(x, y) VALGRIND_HG_ENABLE_CHECKING(x, y)
#else
  #define TOKUBACKUP_VALGRIND_HG_DISABLE_CHECKING(x, y) ((void) x, (void) y)
  #define TOKUBACKUP_VALGRIND_HG_ENABLE_CHECKING(x, y) ((void) x, (void) y)
#endif

#endif  /* BACKUP_HELGPIND_H */
