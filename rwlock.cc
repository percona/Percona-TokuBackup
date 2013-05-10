#include <pthread.h>
#include "check.h"
#include "manager.h"
#include "mutex.h"
#include "rwlock.h"

void prwlock_rdlock(pthread_rwlock_t *lock) throw() {
    int r = pthread_rwlock_rdlock(lock);
    check(r==0);
}

void prwlock_wrlock(pthread_rwlock_t *lock) throw() {
    int r = pthread_rwlock_wrlock(lock);
    check(r==0);
}

void prwlock_unlock(pthread_rwlock_t *lock) throw() {
    int r = pthread_rwlock_unlock(lock);
    check(r==0);
}
