#include "global.h"
#include "object.h"
#include "vm.h"
#include "prim.h"
#include "barrier.h"
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

#if 1
#define DEBUG
#endif

#define TABLE_SIZE	2131

PRIVATE pthread_mutex_t threadtab_mutex;
PRIVATE THREAD threadtab[TABLE_SIZE];
PRIVATE int next_number;

PRIVATE pthread_mutex_t fork_mutex;
PRIVATE pthread_cond_t fork_signal;

#define LOCKTAB()	pthread_mutex_lock(&threadtab_mutex)
#define UNLOCKTAB()	pthread_mutex_unlock(&threadtab_mutex)

PRIVATE void register_thread(THREAD thr) {
  int n = thr->number % TABLE_SIZE;

  LOCKTAB();
#ifdef DEBUG
  printf("%ld regthread %d\n", pthread_self(), thr->number);
#endif
  thr->next = threadtab[n];
  threadtab[n] = thr;
  UNLOCKTAB();
}

PRIVATE void deregister_thread(THREAD thr) {
  int n = thr->number % TABLE_SIZE;
  THREAD curr, prev;

  LOCKTAB();
  prev = NULL;
  curr = threadtab[n];

  while (curr != NULL) {
    if (curr == thr) {
      if (prev == NULL)
	threadtab[n] = curr->next;
      else
	prev->next = curr->next;
#ifdef DEBUG
      printf("deregthread %d\n", thr->number);
#endif
      break;
    }
    prev = curr;
    curr = curr->next;
  }

  thr->next = NULL;
  UNLOCKTAB();
}

PUBLIC void init_thread(void) {
  int i;

  for (i = 0; i < TABLE_SIZE; i++)
    threadtab[i] = 0;

  next_number = 0;

  pthread_mutex_init(&fork_mutex, NULL);
  pthread_cond_init(&fork_signal, NULL);
}

PUBLIC THREAD find_thread_by_tid(pthread_t tid) {
  int i;
  THREAD t = NULL;

  LOCKTAB();
  for (i = 0; i < TABLE_SIZE; i++) {
    t = threadtab[i];

    while (t != NULL) {
      if (t->tid == tid) {
	UNLOCKTAB();
	return t;
      }

      t = t->next;
    }
  }
  UNLOCKTAB();

  return NULL;
}

PUBLIC THREAD find_thread_by_number(int num) {
  THREAD t = NULL;

  LOCKTAB();
  t = threadtab[num % TABLE_SIZE];

  while (t != NULL) {
    if (t->number == num)
      break;

    t = t->next;
  }

  UNLOCKTAB();
  return t;
}

PUBLIC THREAD find_thread_by_vms(VMSTATE vms) {
  int i;
  THREAD t = NULL;

  LOCKTAB();
  for (i = 0; i < TABLE_SIZE; i++) {
    t = threadtab[i];

    while (t != NULL) {
      if (t->vms == vms) {
	UNLOCKTAB();
	return t;
      }

      t = t->next;
    }
  }
  UNLOCKTAB();

  return NULL;
}

typedef struct ThreadArgs {
  int restarting;
  int quota;
  OVECTOR closure;
  THREAD thr;
} ThreadArgs;

PRIVATE void *thread_body(void *args) {
  ThreadArgs *ta = args;
  THREAD thr = ta->thr;

  thr->tid = pthread_self();

  if (!ta->restarting) {
    VMSTATE parentvms = thr->vms;
    VMSTATE vms = allocmem(sizeof(VMstate));
    
    vms->r = (VMREGS) newvector(NUM_VMREGS);
    init_vm(vms);
    thr->vms = vms;

    register_thread(thr);

    vms->c.vm_state = ta->quota;
    vms->r->vm_input = parentvms->r->vm_input;
    vms->r->vm_output = parentvms->r->vm_output;
    vms->r->vm_uid = parentvms->r->vm_effuid;
    vms->r->vm_effuid = parentvms->r->vm_effuid;
    vms->r->vm_trap_closure = parentvms->r->vm_trap_closure;

    apply_closure(vms, ta->closure, newvector_noinit(1));
  } else {
    int i;

    register_thread(thr);

    for (i = 0; i < thr->vms->c.vm_locked_count; i++) {
      LOCK(thr->vms->r->vm_locked);
    }
  }

  protect((OBJ *) &thr->vms->r);

  pthread_mutex_lock(&fork_mutex);
  pthread_cond_signal(&fork_signal);
  pthread_mutex_unlock(&fork_mutex);
  /* NOTE: don't use ta or args from this point on. */

  run_vm(thr->vms);

#ifdef DEBUG
  printf("tid %d state ended as %d\n", thr->number, thr->vms->c.vm_state);
#endif

  while (thr->vms->c.vm_locked_count > 0 && thr->vms->r->vm_locked != NULL) {
    UNLOCK(thr->vms->r->vm_locked);
    thr->vms->c.vm_locked_count--;
  }

  unprotect((OBJ *) &thr->vms->r);

  freemem(thr->vms);
  thr->vms = NULL;
  deregister_thread(thr);
  freemem(thr);

  return NULL;
}

PRIVATE THREAD real_begin_thread(int oldnumber, OVECTOR closure, VMSTATE vms, int quota) {
  ThreadArgs *ta = allocmem(sizeof(ThreadArgs));
  pthread_t tid;

  if (oldnumber == -1) {
    ta->restarting = 0;
    LOCKTAB();
    oldnumber = next_number++;
    UNLOCKTAB();
  } else
    ta->restarting = 1;

  ta->quota = quota;
  ta->closure = closure;
  ta->thr = allocmem(sizeof(Thread));
  ta->thr->number = oldnumber;
  ta->thr->vms = vms;

  pthread_mutex_lock(&fork_mutex);

  pthread_create(&tid, NULL, thread_body, (void *) ta);
  pthread_detach(tid);

  pthread_cond_wait(&fork_signal, &fork_mutex);
  pthread_mutex_unlock(&fork_mutex);

  {
    THREAD ttt = ta->thr;
    freemem(ta);
    return ttt;
  }
}

PUBLIC THREAD begin_thread(OVECTOR closure, VMSTATE parentvms, int cpu_quota) {
  return real_begin_thread(-1, closure, parentvms, cpu_quota);
}

PUBLIC void save_restartable_threads(void *phandle, FILE *f) {
  int i;
  THREAD t = NULL;

  LOCKTAB();
  for (i = 0; i < TABLE_SIZE; i++) {
    t = threadtab[i];

    while (t != NULL) {
      printf("checking thread %d... (%d)", t->number, t->vms->c.vm_state);
      if (t->vms->c.vm_state != VM_STATE_DYING &&
	  t->vms->c.vm_state != VM_STATE_NOQUOTA) {
	printf("saving");
	fwrite(&t->number, sizeof(int), 1, f);
	fwrite(&t->vms->c, sizeof(VMregs_C), 1, f);
	save(phandle, (OBJ) t->vms->r);
      }
      printf("\n");

      t = t->next;
    }
  }
  UNLOCKTAB();

  i = -1;
  fwrite(&i, sizeof(int), 1, f);
}

PUBLIC void load_restartable_threads(void *phandle, FILE *f) {
  int i;

  while (1) {
    VMSTATE vms;

    fread(&i, sizeof(int), 1, f);

    LOCKTAB();
    if (i >= next_number)
      next_number = i + 1;
    UNLOCKTAB();

    if (i == -1)
      break;

    printf("loading thread %d...\n", i);
    vms = allocmem(sizeof(VMstate));
    fread(&vms->c, sizeof(VMregs_C), 1, f);
    vms->r = (VMREGS) load(phandle);
    printf("(vm_locked is %p at %d)\n", vms->r->vm_locked, vms->c.vm_locked_count);
    printf("(vm_state is %d)\n", vms->c.vm_state);

    real_begin_thread(i, NULL, vms, 0);
  }
}
