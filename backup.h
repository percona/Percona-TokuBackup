//
#ifndef BACKUP_H
#define BACKUP_H

#include <sys/types.h>

extern "C" {
int open(const char *file, int oflag, ...);
int close(int fd);
ssize_t write(int fd, const void *buf, size_t nbyte);
int rename (const char *oldpath, const char *newpath);
}

void start_backup(const char*, const char*);
void stop_backup(const char*, const char*);

#endif // end of header guardian.
