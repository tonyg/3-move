#ifndef Recmutex_H
#define Recmutex_H

#if HAVE_NATIVE_RECURSIVE_MUTEXES	/* must include config.h before here */

typedef pthread_mutex_t	recmutex_t;

extern int recmutex_init(recmutex_t *mutex);

#define recmutex_lock(arg)	pthread_mutex_lock(arg)
#define recmutex_unlock(arg)	pthread_mutex_unlock(arg)

#else

typedef struct Recmutex {
  pthread_mutex_t lock;
  pthread_cond_t waiters;
  int counter;
  pthread_t owner;
} recmutex_t;

extern int recmutex_init(recmutex_t *mutex);

extern int recmutex_lock(recmutex_t *mutex);
extern int recmutex_unlock(recmutex_t *mutex);

#endif

#endif
