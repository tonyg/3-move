#ifndef Barrier_H
#define Barrier_H

#include <pthread.h>

struct barrier {
  pthread_mutex_t mutex;
  pthread_cond_t waiters;
  int threshold;
  int waiting;		/* number of threads waiting for threshold to be reached */
  int sleeping;		/* number of threads waiting to be woken up */
};

extern void barrier_init(struct barrier *barrier, int threshold);

extern void barrier_hit(struct barrier *barrier);

extern int barrier_set_threshold(struct barrier *barrier, int threshold);
extern void barrier_inc_threshold(struct barrier *barrier);
extern void barrier_dec_threshold(struct barrier *barrier);

#endif
