/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#ifndef BACKUP_H
#define BACKUP_H

extern "C" {
 // These public API's should be in C if possible
void add_directory(const char* source, const char* destination);
void remove_directory(const char* source, const char* destination);
void start_backup(void);
int  set_source_directory(const char* source);
int  backup_to_this_directory(const char* destination);
void stop_backup(void);
}

#endif // end of header guardian.
