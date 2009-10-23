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

extern void barrier_inc_threshold(struct barrier *barrier);
extern void barrier_dec_threshold(struct barrier *barrier);

#endif
