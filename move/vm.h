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

#ifndef VM_H
#define VM_H

#include <stdio.h>

/************************************************************************/
/* VM register file							*/

#define NUM_VMREGS	12
typedef struct VMregs {
  Obj		_;

  OBJ		vm_acc;		/* Accumulator */
  BVECTOR	vm_code;	/* Code block */
  VECTOR	vm_env;		/* Environment */
  VECTOR	vm_lits;	/* Literal table */
  OBJECT	vm_self;	/* Self object */
  VECTOR	vm_stack;	/* Stack */
  OVECTOR	vm_frame;	/* Frame stack */
  OVECTOR	vm_method;	/* The callable that this frame is executing */

  OVECTOR	vm_trap_closure;	/* closure to call on VM trap, eg vm_raise etc. */
  OBJECT	vm_uid;		/* Real owner of this thread */
  OBJECT	vm_effuid;	/* Effective owner of this code */
  OBJECT	vm_locked;	/* Object this thread holds the lock for, or NULL */
} VMregs, *VMREGS;

typedef struct VMregs_C {
  int32_t	vm_ip;		/* Instruction pointer */
  int32_t	vm_top;		/* Top of stack (ascending, empty) */
  int		vm_state;	/* Status of Virtual Machine, see below */
  int		vm_locked_count;	/* Recursive lock count of vm_locked */
} VMregs_C, *VMREGS_C;

#define VM_STATE_DYING		0	/* thread should exit asap */
#define VM_STATE_DAEMON		-1	/* thread should run forever, and be checkpointed */
#define VM_STATE_NOQUOTA	-2	/* thread should run forever, but be discarded */
/* or if vm_state > 0, then that's the number of cycles of VM 'CPU-time' it has left. */

typedef struct VMstate {
  VMREGS r;
  VMregs_C c;
} VMstate, *VMSTATE;

#define VM_DEFAULT_CPU_QUOTA	15000	/* 15k cycles */

/************************************************************************/
/* VM API								*/

extern char *checkpoint_filename;
extern uint32_t synch_bitmap;	/* If nonzero, synchronization desired */

/* Values for synch_bitmap: */
#define SYNCH_GC		0x00000001	/* Force a GC */
#define SYNCH_CHECKPOINT	0x00000002	/* Synchronize for a checkpoint */

extern BVECTOR bvector_concat(BVECTOR a, BVECTOR b);
extern VECTOR vector_concat(VECTOR a, VECTOR b);

extern void init_vm_global(void);
extern void vm_restore_from(FILE *f, int load_threads);
extern void vm_poll_gc(void);
extern void gc_reach_safepoint(void);

extern void init_vm(VMSTATE vms);

extern OVECTOR getcont_from(VMSTATE vms);
extern void apply_closure(VMSTATE vms, OVECTOR closure, VECTOR argvec);
extern void vm_raise(VMSTATE vms, OBJ exception, OBJ arg);
extern int run_vm(VMSTATE vms);
	/* returns 1 if thread completed, 0 for end-of-timeslice */
extern void push_frame(VMSTATE vms);

#endif
