#ifndef Recmutex_H
#define Recmutex_H

typedef struct Recmutex {
  int counter;
  int owner_threadnum;
} recmutex_t;

extern int recmutex_init(recmutex_t *mutex);

extern int recmutex_lock(recmutex_t *mutex);
extern int recmutex_unlock(recmutex_t *mutex);

#endif
