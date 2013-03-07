/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#ifndef BACKUP_INTEGRATION_TEST_H
#define BACKUP_INTEGRATION_TEST_H

void open_write_close();
void read_and_seek();
void copy_files();
int backup_sub_dirs();
void test_truncate();

#endif // BACKUP_INTEGRATION_TEST_H
