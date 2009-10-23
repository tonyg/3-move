/*
 * 3-MOVE, a multi-user networked online text-based programmable virtual environment
 * Copyright 1997, 1998, 1999, 2003, 2005, 2008, 2009 Tony Garnock-Jones <tonyg@kcbbs.gen.nz>
 *
 * 3-MOVE is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * 3-MOVE is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with 3-MOVE.  If not, see <http://www.gnu.org/licenses/>.
 */

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

#ifdef __GNUC__
#define INLINE inline
#else
#define INLINE
#endif

static INLINE void wait_for_wakeup(struct barrier *barrier) {
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

#if 0
/**************************************************************************/
/* NOONE USES THIS FUNCTION SO IT'S #if'ed OUT! */
/**************************************************************************/
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
/**************************************************************************/
#endif

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
