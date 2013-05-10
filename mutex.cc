#include <pthread.h>

#include "manager.h"

#define NO_DEPRECATE_PTHREAD_MUTEXES
#include "mutex.h"
#include "check.h"

void pmutex_lock(pthread_mutex_t *mutex) throw() {
    int r = pthread_mutex_lock(mutex);
    check(r==0);
}

void pmutex_unlock(pthread_mutex_t *mutex) throw() {
    int r = pthread_mutex_unlock(mutex);
    check(r==0);
}
