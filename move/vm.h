#ifndef VM_H
#define VM_H

#include <stdio.h>

/************************************************************************/
/* VM register file							*/

#define NUM_VMREGS	14
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
  OVECTOR	vm_input;	/* Input connection */
  OVECTOR	vm_output;	/* Output connection */
} VMregs, *VMREGS;

typedef struct VMregs_C {
  i32		vm_ip;		/* Instruction pointer */
  i32		vm_top;		/* Top of stack (ascending, empty) */
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
extern u32 synch_bitmap;	/* If nonzero, synchronization desired */

/* Values for synch_bitmap: */
#define SYNCH_GC		0x00000001	/* Force a GC */
#define SYNCH_CHECKPOINT	0x00000002	/* Synchronize for a checkpoint */

extern BVECTOR bvector_concat(BVECTOR a, BVECTOR b);
extern VECTOR vector_concat(VECTOR a, VECTOR b);

extern void init_vm_global(void);
extern void vm_restore_from(FILE *f);
extern void *vm_gc_thread_main(void *arg);
extern void make_gc_thread_exit(void);
extern void gc_inc_safepoints(void);
extern void gc_dec_safepoints(void);
extern void gc_reach_safepoint(void);

extern void init_vm(VMSTATE vms);

extern OVECTOR getcont_from(VMSTATE vms);
extern void apply_closure(VMSTATE vms, OVECTOR closure, VECTOR argvec);
extern void vm_raise(VMSTATE vms, OBJ exception, OBJ arg);
extern void run_vm(VMSTATE vms);
extern void push_frame(VMSTATE vms);

#endif
