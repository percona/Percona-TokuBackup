#include <pthread.h>

#include "manager.h"

#define NO_DEPRECATE_PTHREAD_MUTEXES
#include "mutex.h"
#include "check.h"

void pmutex_lock(pthread_mutex_t *mutex) {
    int r = pthread_mutex_lock(mutex);
    check(r==0);
}

void pmutex_unlock(pthread_mutex_t *mutex) {
    int r = pthread_mutex_unlock(mutex);
    check(r==0);
}

int pmutex_lock_c(pthread_mutex_t *mutex) {
    pmutex_lock(mutex);
    return 0;
}

int pmutex_unlock_c(pthread_mutex_t *mutex) {
    pmutex_unlock(mutex);
    return 0;
}
