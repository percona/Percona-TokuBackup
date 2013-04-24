#include <pthread.h>
#include "manager.h"
#include "mutex.h"
#include "rwlock.h"

static void handle_error(int r) {
  the_manager.fatal_error(r, "pthread rwlock failure");
}

int prwlock_rdlock(pthread_rwlock_t *lock) {
    int r = pthread_rwlock_rdlock(lock);
    if (r!=0) {
      handle_error(r);
    }
    return r;
}

int prwlock_wrlock(pthread_rwlock_t *lock) {
    int r = pthread_rwlock_wrlock(lock);
    if (r!=0) {
      handle_error(r);
    }
    return r;
}

int prwlock_unlock(pthread_rwlock_t *lock) {
    int r = pthread_rwlock_unlock(lock);
    if (r!=0) {
      handle_error(r);
    }
    return r;
}
