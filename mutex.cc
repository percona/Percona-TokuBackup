#include <pthread.h>

#include "manager.h"

#define NO_DEPRECATE_PTHREAD_MUTEXES
#include "mutex.h"

int pmutex_lock(pthread_mutex_t *mutex) {
    int r = pthread_mutex_lock(mutex);
    if (r!=0) {
        the_manager.fatal_error(r, "pthread lock failure");
    }
    return r;
}

int pmutex_unlock(pthread_mutex_t *mutex) {
    int r = pthread_mutex_unlock(mutex);
    if (r!=0) {
        the_manager.fatal_error(r, "pthread lock failure.");
    }
    return r;
}
