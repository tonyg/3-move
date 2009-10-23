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

#ifndef Thread_H
#define Thread_H

#define BLOCK_CTXT_NONE			0	/* not blocked - context is invalid */
#define BLOCK_CTXT_READLINE		1	/* blocked reading a line -
						   context is vector with:
						   0	BVECTOR	buffer
						   1	number	offset
						   2	number	size
						   3	OVECTOR	connection
						   */
#define BLOCK_CTXT_ACCEPT		2	/* blocked accepting a TCP connection -
						   context is:
						   OVECTOR	server-socket-connection
						   */
#define BLOCK_CTXT_LOCKING		3	/* blocked attempting to lock an object -
						   context is:
						   OBJECT	the object in question
						   */

typedef struct Thread {
  VMSTATE vms;
  int number;
  struct Thread *next;	/* in hashtable */

  int contextkind;
  OBJ context;

  struct thread_q *queue;	/* detail about position in runQ, blockQ, or sleepQ */
  struct Thread *next_q;
  struct Thread *prev_q;
} Thread, *THREAD;

/* Note: THREAD->contextkind holds the time of reawakening of the thread
   if the thread is sleeping due to a call to sleep_thread().
   */

typedef struct ThreadStat {
  int number;
  OBJECT owner;
  int sleeping;		/* 0 -> run_q or block_q, 1-> sleep_q */
  int status;		/* BLOCK_CTXT_... or time of awakening */
} ThreadStat;

extern THREAD current_thread;

extern void init_thread(void);

extern int count_active_threads(void);

extern THREAD find_thread_by_number(int number);
extern THREAD find_thread_by_vms(VMSTATE vms);

extern THREAD begin_thread(OVECTOR closure, VMSTATE parentvms, int cpu_quota);

/* These next three routines called from run_main_loop() in main.c */
extern int run_run_queue(void);		/* 1 => some threads ran, 0 => runQ empty */
extern void run_blocked_queue(void);	/* attempts to resume blocked/sleeping threads */
extern void block_until_event(void);	/* uses select() to wait for an alarm or some io */

extern void block_thread(int contextkind, OBJ context);	/* io-blocks this thread */
extern void sleep_thread(int seconds);	/* sleeps this thread, accurate to +/- 1 sec. */
extern void unblock_thread(THREAD thr);	/* called from within conn_resume_XXXX routines */
extern int thread_is_blocked(THREAD thr);

extern void load_restartable_threads(void *phandle, FILE *f);
extern void save_restartable_threads(void *phandle, FILE *f);

extern ThreadStat *get_thread_stats(void);
extern int get_thread_status(int n, ThreadStat *status);

#endif
