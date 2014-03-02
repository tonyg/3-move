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
#include "object.h"
#include "vm.h"
#include "prim.h"
#include "gc.h"
#include "primload.h"
#include "scanner.h"
#include "parser.h"
#include "conn.h"
#include "thread.h"
#include "persist.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>

#if 1
#define DEBUG
#endif

#define TABLE_SIZE	2131

PUBLIC THREAD current_thread;	/* for use by primitives, run_vm, et. al. */

typedef struct thread_q { THREAD h, t; } thread_q, *THREAD_Q;

PRIVATE int num_active;		/* number of threads in all queues combined */
PRIVATE thread_q run_q;		/* runnable threads */
PRIVATE thread_q block_q;	/* io-blocked threads */
PRIVATE thread_q sleep_q;	/* sleeping threads */

PRIVATE THREAD threadtab[TABLE_SIZE];
PRIVATE int next_number;

PRIVATE void enq(THREAD_Q q, THREAD t) {
  if (!q)
    return;

  t->prev_q = NULL;
  t->next_q = q->h;

  if (q->h)
    q->h->prev_q = t;
  else
    q->t = t;
  q->h = t;

  t->queue = q;
}

PRIVATE void deq(THREAD_Q q, THREAD t) {
  if (!q)
    return;

  if (t->prev_q)
    t->prev_q->next_q = t->next_q;
  else
    q->h = t->next_q;

  if (t->next_q)
    t->next_q->prev_q = t->prev_q;
  else
    q->t = t->prev_q;

  t->queue = NULL;
}

PRIVATE void register_thread(THREAD thr) {
  int n = thr->number % TABLE_SIZE;

#ifdef DEBUG
  printf("%p regthread %d\n", current_thread, thr->number);
#endif
  thr->next = threadtab[n];
  threadtab[n] = thr;

  num_active++;
  enq(&run_q, thr);
}

PRIVATE void deregister_thread(THREAD thr) {
  int n = thr->number % TABLE_SIZE;
  THREAD curr, prev;

  prev = NULL;
  curr = threadtab[n];

  while (curr != NULL) {
    if (curr == thr) {
      if (prev == NULL)
	threadtab[n] = curr->next;
      else
	prev->next = curr->next;

      num_active--;
      deq(thr->queue, thr);
#ifdef DEBUG
      printf("deregthread %d\n", thr->number);
#endif
      break;
    }
    prev = curr;
    curr = curr->next;
  }

  thr->next = NULL;
}

PUBLIC void init_thread(void) {
  int i;

  for (i = 0; i < TABLE_SIZE; i++)
    threadtab[i] = NULL;

  next_number = 1;
  current_thread = NULL;

  run_q.h = run_q.t = NULL;
  block_q.h = block_q.t = NULL;
  sleep_q.h = sleep_q.t = NULL;
}

PUBLIC int count_active_threads(void) {
  return num_active;
}

PUBLIC THREAD find_thread_by_number(int num) {
  THREAD t = NULL;

  t = threadtab[num % TABLE_SIZE];

  while (t != NULL) {
    if (t->number == num)
      break;

    t = t->next;
  }

  return t;
}

PUBLIC THREAD find_thread_by_vms(VMSTATE vms) {
  int i;
  THREAD t = NULL;

  for (i = 0; i < TABLE_SIZE; i++) {
    t = threadtab[i];

    while (t != NULL) {
      if (t->vms == vms) {
	return t;
      }

      t = t->next;
    }
  }

  return NULL;
}

PRIVATE THREAD real_begin_thread(int oldnumber, OVECTOR closure, VMSTATE vms, int quota) {
  int restarting;
  THREAD thr;

  if (oldnumber == -1) {
    restarting = 0;
    oldnumber = next_number++;
  } else
    restarting = 1;

  thr = allocmem(sizeof(Thread));
  thr->number = oldnumber;
  thr->vms = vms;
  thr->contextkind = BLOCK_CTXT_NONE;
  thr->context = NULL;

  if (!restarting) {
    VMSTATE parentvms = vms;

    vms = allocmem(sizeof(VMstate));

    vms->r = (VMREGS) newvector(NUM_VMREGS);
    init_vm(vms);
    thr->vms = vms;

    register_thread(thr);

    vms->c.vm_state = quota;
    vms->r->vm_uid = parentvms->r->vm_effuid;
    vms->r->vm_effuid = parentvms->r->vm_effuid;
    vms->r->vm_trap_closure = parentvms->r->vm_trap_closure;

    apply_closure(vms, closure, newvector_noinit(1));
  } else {
    int i;

    register_thread(thr);

    for (i = 0; i < thr->vms->c.vm_locked_count; i++) {
      LOCK(thr->vms->r->vm_locked);
    }
  }

  protect((OBJ *) &thr->vms->r);
  protect(&thr->context);

  return thr;
}

PUBLIC THREAD begin_thread(OVECTOR closure, VMSTATE parentvms, int cpu_quota) {
  return real_begin_thread(-1, closure, parentvms, cpu_quota);
}

PUBLIC int run_run_queue(void) {
  THREAD thr;

  thr = run_q.h;

  if (thr == NULL)
    return 0;		/* 0 => runQ empty */

  while (thr != NULL) {
    THREAD next = thr->next_q;

    current_thread = thr;
    if (run_vm(thr->vms)) {
#ifdef DEBUG
      printf("tid %d state ended as %d\n", thr->number, thr->vms->c.vm_state);
#endif

      while (thr->vms->c.vm_locked_count > 0 && thr->vms->r->vm_locked != NULL) {
	UNLOCK(thr->vms->r->vm_locked);
	thr->vms->c.vm_locked_count--;
      }

      unprotect(&thr->context);
      unprotect((OBJ *) &thr->vms->r);

      freemem(thr->vms);
      thr->vms = NULL;
      deregister_thread(thr);
      freemem(thr);
    }

    thr = next;
  }

  current_thread = NULL;
  return 1;
}

PUBLIC void run_blocked_queue(void) {
  THREAD thr;
  time_t time_now = time(NULL);

  thr = sleep_q.h;
  while (thr != NULL) {
    THREAD next = thr->next_q;

    if (thr->contextkind <= time_now) {
      unblock_thread(thr);
      thr->vms->r->vm_acc = true;	/* for sleepFun primitive. */
    }

    thr = next;
  }

  thr = block_q.h;
  while (thr != NULL) {
    THREAD next = thr->next_q;

    current_thread = thr;
    switch (thr->contextkind) {
      case BLOCK_CTXT_READLINE:
	conn_resume_readline((VECTOR) thr->context);
	break;

      case BLOCK_CTXT_ACCEPT:
	conn_resume_accept((OVECTOR) thr->context);
	break;

      case BLOCK_CTXT_LOCKING:
	if (LOCK((OBJECT) thr->context) >= 0) {
	  deq(&block_q, thr);
	  enq(&run_q, thr);
	}
	break;

      case BLOCK_CTXT_NONE:
      default:
	fprintf(stderr, "Invalid BLOCK_CTXT %d in run_blocked_queue\n",
		thr->contextkind);
	exit(MOVE_EXIT_MEMORY_ODDNESS);
    }

    thr = next;
  }

  current_thread = NULL;
}

PUBLIC void block_until_event(void) {
  THREAD thr;
  time_t time_now;
  time_t nearest_alarm_time = 0;
  int sel_n = 0;
  fd_set set;
  struct timeval tv;

  time_now = time(NULL);

  thr = sleep_q.h;
  while (thr != NULL) {
    time_t time_then = thr->contextkind;

    thr = thr->next_q;

    if (time_then <= time_now)
      return;	/* don't block, an alarm has gone off */

    if (!nearest_alarm_time || time_then < nearest_alarm_time)
      nearest_alarm_time = time_then;
  }

  FD_ZERO(&set);

  thr = block_q.h;
  while (thr != NULL) {
    OVECTOR conn = NULL;
    int the_fd;

    switch (thr->contextkind) {
      case BLOCK_CTXT_READLINE:
	conn = (OVECTOR) AT((VECTOR) thr->context, 3);
	break;

      case BLOCK_CTXT_ACCEPT:
	conn = (OVECTOR) thr->context;
	break;

      case BLOCK_CTXT_LOCKING:
	break;

      case BLOCK_CTXT_NONE:
      default:
	fprintf(stderr, "Invalid BLOCK_CTXT %d in block_until_event\n",
		thr->contextkind);
	exit(MOVE_EXIT_MEMORY_ODDNESS);
    }

    thr = thr->next_q;

    if (!conn || AT(conn, CO_TYPE) != MKNUM(CONN_FILE))
      continue;

    /* Now we know that we have a connection to think about,
       and that the connection has an associated fd. */

    the_fd = NUM(AT(conn, CO_HANDLE));
    FD_SET(the_fd, &set);
    if (the_fd >= sel_n)
      sel_n = the_fd + 1;
  }

  tv.tv_sec = nearest_alarm_time - time_now;
  tv.tv_usec = 0;

  if (select(sel_n, &set, NULL, &set, nearest_alarm_time ? &tv : NULL) < 0) {
    int e = errno;

    if (e != EAGAIN) {
      fprintf(stderr, "Error in select(): errno = %d\n", e);
      exit(MOVE_EXIT_ERROR);
    }
  }
}

PUBLIC void block_thread(int contextkind, OBJ context) {
  /* Don't bother if there's no current thread. */
  if (!current_thread)
    return;

  current_thread->contextkind = contextkind;
  current_thread->context = context;

  deq(current_thread->queue, current_thread);
  enq(&block_q, current_thread);
}

PUBLIC void sleep_thread(int seconds) {
  time_t time_now;

  if (!current_thread)
    return;

  time_now = time(NULL);
  current_thread->contextkind = time_now + seconds;

  deq(current_thread->queue, current_thread);
  enq(&sleep_q, current_thread);
}

PUBLIC void unblock_thread(THREAD thr) {
  if (!thr)
    return;

  if (thr->vms->r->vm_acc == yield_thread)
    thr->vms->r->vm_acc = undefined;

  thr->contextkind = BLOCK_CTXT_NONE;

  deq(thr->queue, thr);
  enq(&run_q, thr);
}

PUBLIC int thread_is_blocked(THREAD thr) {
  return thr->queue != &run_q;
}

PUBLIC void save_restartable_threads(void *phandle, FILE *f) {
  int i;
  THREAD t = NULL;

  for (i = 0; i < TABLE_SIZE; i++) {
    t = threadtab[i];

    while (t != NULL) {
      printf("checking thread %d... (%d)", t->number, t->vms->c.vm_state);
      if (t->vms->c.vm_state != VM_STATE_DYING &&
	  t->vms->c.vm_state != VM_STATE_NOQUOTA) {
	printf("saving");
	{
	  int tmp = htonl(t->number);
	  fwrite(&tmp, sizeof(int), 1, f);
	}
	{
	  VMregs_C tmp;
	  tmp.vm_ip = htonl(t->vms->c.vm_ip);
	  tmp.vm_top = htonl(t->vms->c.vm_top);
	  tmp.vm_state = htonl(t->vms->c.vm_state);
	  tmp.vm_locked_count = htonl(t->vms->c.vm_locked_count);
	  fwrite(&tmp, sizeof(VMregs_C), 1, f);
	}
	save(phandle, (OBJ) t->vms->r);
      }
      printf("\n");

      t = t->next;
    }
  }

  i = -1;
  fwrite(&i, sizeof(int), 1, f);
}

PUBLIC void load_restartable_threads(void *phandle, FILE *f) {
  int i;
  int use_net_byte_ordering = (get_handle_dbfmt(phandle) == DBFMT_NET_32);

  while (1) {
    VMSTATE vms;

    checked_fread(&i, sizeof(int), 1, f);
    if (use_net_byte_ordering)
      i = ntohl(i);

    if (i >= next_number)
      next_number = i + 1;

    if (i == -1)
      break;

    printf("loading thread %d...\n", i);
    vms = allocmem(sizeof(VMstate));
    checked_fread(&vms->c, sizeof(VMregs_C), 1, f);
    if (use_net_byte_ordering) {
      vms->c.vm_ip = ntohl(vms->c.vm_ip);
      vms->c.vm_top = ntohl(vms->c.vm_top);
      vms->c.vm_state = ntohl(vms->c.vm_state);
      vms->c.vm_locked_count = ntohl(vms->c.vm_locked_count);
    }
    vms->r = (VMREGS) load(phandle);
    /*
      printf("(vm_locked is %p at %d)\n", vms->r->vm_locked, vms->c.vm_locked_count);
      printf("(vm_state is %d)\n", vms->c.vm_state);
      */

    if (vms->r->vm_acc == yield_thread)
      vms->r->vm_acc = undefined;

    real_begin_thread(i, NULL, vms, 0);
  }
}

PUBLIC ThreadStat *get_thread_stats(void) {
  int i, n;
  ThreadStat *tab = allocmem(sizeof(ThreadStat) * (num_active + 1));

  tab[0].number = num_active;

  for (i = 0, n = 1; i < TABLE_SIZE; i++) {
    THREAD t = threadtab[i];

    while (t != NULL) {
      tab[n].number = t->number;
      tab[n].owner = t->vms->r->vm_uid;
      tab[n].sleeping = (t->queue == &sleep_q);
      tab[n].status = t->contextkind;

      t = t->next;
      n++;
    }
  }

  return tab;
}

PUBLIC int get_thread_status(int n, ThreadStat *status) {
  THREAD t;

  t = find_thread_by_number(n);

  if (t == NULL)
    return 0;
  else {
    status->number = t->number;
    status->owner = t->vms->r->vm_uid;
    status->sleeping = (t->queue == &sleep_q);
    status->status = t->contextkind;
    return 1;
  }
}
