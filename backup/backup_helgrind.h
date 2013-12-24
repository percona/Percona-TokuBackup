/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."

#ifndef BACKUP_HELGRIND_H
#define BACKUP_HELGRIND_H

#if BACKUP_USE_VALGRIND
  #include <valgrind/helgrind.h>
  #define TOKUBACKUP_VALGRIND_HG_DISABLE_CHECKING(x, y) VALGRIND_HG_DISABLE_CHECKING(x, y)
#else
  #define TOKUBACKUP_VALGRIND_HG_DISABLE_CHECKING(x, y) ((void) (x, y))
#endif

#endif  /* BACKUP_HELGPIND_H */
