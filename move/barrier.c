#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "barrier.h"

#if 0
#define DEBUG
#endif

void barrier_init(struct barrier *barrier, int threshold) {
  pthread_mutex_init(&barrier->mutex, NULL);
  pthread_cond_init(&barrier->waiters, NULL);
  barrier->threshold = threshold;
  barrier->waiting = 0;
  barrier->sleeping = 0;
}

static inline void wait_for_wakeup(struct barrier *barrier) {
  if (barrier->sleeping == barrier->waiting) {
    barrier->sleeping = barrier->waiting = 0;
#ifdef DEBUG
    printf("%d sleeping threshold reached, broadcasting\n", pthread_self());
#endif
    pthread_cond_broadcast(&barrier->waiters);
  } else {
#ifdef DEBUG
    printf("%d goes to sleep waiting for everyone to wake up\n", pthread_self());
#endif
    pthread_cond_wait(&barrier->waiters, &barrier->mutex);
  }
}

void barrier_hit(struct barrier *barrier) {
  pthread_mutex_lock(&barrier->mutex);

  barrier->waiting++;
#ifdef DEBUG
  printf("%d hit %p w %d t %d ", pthread_self(), barrier, barrier->waiting, barrier->threshold);
#endif

  if (barrier->waiting >= barrier->threshold) {
#ifdef DEBUG
    printf("awoke\n");
#endif
    pthread_cond_broadcast(&barrier->waiters);
  } else {
    while (barrier->waiting < barrier->threshold) {
#ifdef DEBUG
      printf("%d sleeping w %d t %d\n", pthread_self(), barrier->waiting, barrier->threshold);
#endif
      pthread_cond_wait(&barrier->waiters, &barrier->mutex);
#ifdef DEBUG
      printf("(%d WOKE UP!)\n", pthread_self());
#endif
    }
  }

  barrier->sleeping++;
  wait_for_wakeup(barrier);

  pthread_mutex_unlock(&barrier->mutex);
#ifdef DEBUG
  printf("%d leaving hit %p\n", pthread_self(), barrier);
#endif
}

int barrier_set_threshold(struct barrier *barrier, int threshold) {
  int old;

  pthread_mutex_lock(&barrier->mutex);

  while (barrier->sleeping != 0)
    pthread_cond_wait(&barrier->waiters, &barrier->mutex);

  old = barrier->threshold;
  barrier->threshold = threshold;

  if (barrier->waiting >= barrier->threshold) {
#ifdef DEBUG
    printf("(%d awoke-set-threshold %p to %d)\n", pthread_self(), barrier, threshold);
#endif
    pthread_cond_broadcast(&barrier->waiters);
    wait_for_wakeup(barrier);
  }

  pthread_mutex_unlock(&barrier->mutex);

  return old;
}

void barrier_inc_threshold(struct barrier *barrier) {
  pthread_mutex_lock(&barrier->mutex);
  barrier->threshold++;
#ifdef DEBUG
  printf("(%d inc-threshold %p to %d)\n", pthread_self(), barrier, barrier->threshold);
#endif
  pthread_mutex_unlock(&barrier->mutex);
}

void barrier_dec_threshold(struct barrier *barrier) {
  pthread_mutex_lock(&barrier->mutex);
  while (barrier->sleeping != 0)
    pthread_cond_wait(&barrier->waiters, &barrier->mutex);
  barrier->threshold--;
  if (barrier->waiting >= barrier->threshold) {
#ifdef DEBUG
    printf("(%d awoke-dec-threshold %p to %d)\n", pthread_self(), barrier, barrier->threshold);
#endif
    pthread_cond_broadcast(&barrier->waiters);
    wait_for_wakeup(barrier);
  }
  pthread_mutex_unlock(&barrier->mutex);
}
