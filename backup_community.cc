/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
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
