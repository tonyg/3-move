#include <pthread.h>

#include "global.h"
#include "config.h"
#include "recmutex.h"

#if HAVE_NATIVE_RECURSIVE_MUTEXES

PUBLIC int recmutex_init(recmutex_t *mutex) {
  pthread_mutexattr_t attr;

  pthread_mutexattr_init(&attr);
  pthread_mutexattr_setkind(&attr, PTHREAD_MUTEX_RECURSIVE_NP);

  return pthread_mutex_init(mutex, &attr);
}

#else

PUBLIC int recmutex_init(recmutex_t *mutex) {
  pthread_mutex_init(&mutex->lock, NULL);
  pthread_cond_init(&mutex->waiters, NULL);
  mutex->counter = 0;
  mutex->owner = pthread_self();
  return 0;
}

PUBLIC int recmutex_lock(recmutex_t *mutex) {
  pthread_t self = pthread_self();

  pthread_mutex_lock(&mutex->lock);

  if (mutex->counter != 0 && mutex->owner != self)
    pthread_cond_wait(&mutex->waiters, &mutex->lock);

  mutex->counter++;
  mutex->owner = self;

  pthread_mutex_unlock(&mutex->lock);
  return 0;
}

PUBLIC int recmutex_unlock(recmutex_t *mutex) {
  pthread_mutex_lock(&mutex->lock);

  if (--mutex->counter <= 0) {
    mutex->counter = 0;
    pthread_cond_signal(&mutex->waiters);
  }

  pthread_mutex_unlock(&mutex->lock);
  return 0;
}

#endif
