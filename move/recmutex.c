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

#include "global.h"
#include "config.h"
#include "recmutex.h"
#include "object.h"
#include "vm.h"
#include "thread.h"

PUBLIC int recmutex_init(recmutex_t *mutex) {
  mutex->counter = 0;
  mutex->owner_threadnum = current_thread ? current_thread->number : 0;
  return 0;
}

PUBLIC int recmutex_lock(recmutex_t *mutex) {
  int self = current_thread ? current_thread->number : 0;

  if (mutex->counter != 0 && mutex->owner_threadnum != self)
    return -1;	/* EAGAIN, more or less */

  mutex->counter++;
  mutex->owner_threadnum = self;
  return 0;
}

PUBLIC int recmutex_unlock(recmutex_t *mutex) {
  if (--mutex->counter <= 0) {
    mutex->counter = 0;
    mutex->owner_threadnum = 0;
  }
  return 0;
}
