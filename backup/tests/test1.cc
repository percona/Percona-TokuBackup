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

#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include "backup.h"

#define SRC "b2_src"
#define DST "b2_dst"

//
static void setup_destination()
{
    system("rm -rf " DST);
}

//
static void setup_source()
{
    system("rm -rf " SRC);
    system("mkdir " SRC);
    system("touch " SRC "/foo");
    system("echo hello > " SRC "/bar.data");
    system("mkdir " SRC "/subdir");
    system("echo there > " SRC "/subdir/sub.data");
}

//
int main(int argc, char *argv[])
{
    argc = argc;
    argv = argv;
    int result = 0;
    setup_source();
    setup_destination();
    add_directory(SRC, DST);
    start_backup();
    
    int fd = 0;
    fd = open(SRC "/bar.data", O_WRONLY);
    check(fd >= 0);
    result = write(fd, "goodbye\n", 8);
    check(result == 8);
    result = close(fd);
    check(result == 0);

    stop_backup();
    return result;
}
