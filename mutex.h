#ifndef MUTEX_H
#define MUTEX_H

#include <pthread.h>

#ifndef NO_DEPRECATE_PTHREAD_MUTEXES
extern int pthread_mutex_lock(pthread_mutex_t *) __attribute__((deprecated));
extern int pthread_mutex_unlock(pthread_mutex_t *) __attribute__((deprecated));
#endif

extern int pmutex_lock(pthread_mutex_t *); // Reports any errors to the_manager and returns the error code.
extern int pmutex_unlock(pthread_mutex_t *); // Reports any errors to the_manager and returns the error code.
#endif
