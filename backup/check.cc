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

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#include "check.h"
#include "manager.h"

void check_fun(long predicate, const char *expr, const backtrace &bt) throw() {
    if (!predicate) {
        the_manager.kill();
        int e = errno;
        fprintf(stderr, "check(%s) failed\n", expr);
        fprintf(stderr, "errno=%d\n", e);
        const backtrace *btp=&bt;
        fprintf(stderr, "backtrace:\n");
        while (btp) {
            fprintf(stderr, " %s:%d (%s)\n", btp->file, btp->line, btp->fun);
            btp = btp->prev;
        }
        abort();
    }
}
