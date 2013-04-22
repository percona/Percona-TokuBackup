#ifndef RWLOCK_H
#define RWLOCK_H

#include <pthread.h>

extern int prwlock_rdlock(pthread_rwlock_t *);
extern int prwlock_wrlock(pthread_rwlock_t *);
extern int prwlock_unlock(pthread_rwlock_t *);

#endif // End of header guardian.
