#ifndef RWLOCK_H
#define RWLOCK_H

#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id: fmap.h 55798 2013-04-20 12:33:47Z christianrober $"

#include <pthread.h>

extern int prwlock_rdlock(pthread_rwlock_t *);
extern int prwlock_wrlock(pthread_rwlock_t *);
extern int prwlock_unlock(pthread_rwlock_t *);

#endif // End of header guardian.
